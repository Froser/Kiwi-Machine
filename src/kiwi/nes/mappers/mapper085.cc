// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "nes/mappers/mapper085.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kCHRRAMSize = 0x2000;

}  // namespace

Mapper085::Mapper085(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 85);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 4 * kPRGBankSize);

  if (rom_data()->submapper == 1) {
    wiring_ = Wiring::kA3;
  } else if (rom_data()->submapper == 2) {
    wiring_ = Wiring::kA4;
  }

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = character_ram_.size() / kCHRBankSize;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  EnsurePRGRAM(0x2000);
  CHECK(!audio_.init());
  Reset();
}

Mapper085::~Mapper085() = default;

void Mapper085::Reset() {
  std::fill(std::begin(prg_banks_), std::end(prg_banks_), 0);
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  control_ = 0;
  irq_reload_ = 0;
  irq_counter_ = 0;
  irq_prescaler_ = 0;
  irq_enabled_ = false;
  irq_enabled_after_ack_ = false;
  irq_cycle_mode_ = false;
  audio_.reset();
  audio_cycles_ = 0;
}

void Mapper085::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  const Address audio_reg = address & 0xf030;
  if (audio_reg == 0x9010) {
    if (HasExpansionAudio() && !(control_ & 0x40))
      audio_.write_reg(value);
    return;
  }
  if (audio_reg == 0x9030) {
    if (HasExpansionAudio() && !(control_ & 0x40))
      audio_.write_data(static_cast<blip_time_t>(audio_cycles_), value);
    return;
  }

  const Address reg = TranslateAddress(address);
  switch (reg) {
    case 0x8000:
      prg_banks_[0] = value & 0x3f;
      break;
    case 0x8010:
      prg_banks_[1] = value & 0x3f;
      break;
    case 0x9000:
      prg_banks_[2] = value & 0x3f;
      break;
    case 0xa000:
    case 0xa010:
    case 0xb000:
    case 0xb010:
    case 0xc000:
    case 0xc010:
    case 0xd000:
    case 0xd010: {
      const size_t window = ((reg >> 12) - 0x0a) * 2 + ((reg >> 4) & 0x01);
      chr_banks_[window] = value;
      break;
    }
    case 0xe000: {
      constexpr NametableMirroring kMirroringModes[] = {
          NametableMirroring::kVertical,
          NametableMirroring::kHorizontal,
          NametableMirroring::kOneScreenLower,
          NametableMirroring::kOneScreenHigher,
      };
      const NametableMirroring old_mirroring = kMirroringModes[control_ & 0x03];
      control_ = value;
      if (value & 0x40)
        audio_.reset(static_cast<blip_time_t>(audio_cycles_));
      if (old_mirroring != kMirroringModes[control_ & 0x03] &&
          mirroring_changed_callback()) {
        mirroring_changed_callback().Run();
      }
      break;
    }
    case 0xe010:
      irq_reload_ = value;
      break;
    case 0xf000:
      SetIRQControl(value);
      break;
    case 0xf010:
      AcknowledgeIRQ();
      break;
  }
}

Byte Mapper085::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t window = (address - 0x8000) / kPRGBankSize;
  const size_t bank =
      (window < 3 ? prg_banks_[window] : prg_bank_count_ - 1) % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x1fff)];
}

void Mapper085::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[GetCHRAddress(address)] = value;
}

Byte Mapper085::ReadCHR(Address address) {
  const size_t index = GetCHRAddress(address);
  return character_ram_.empty() ? rom_data()->CHR[index]
                                : character_ram_[index];
}

uint32_t Mapper085::GetAbsoluteCHRAddress(Address address) {
  return static_cast<uint32_t>(GetCHRAddress(address));
}

void Mapper085::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 || address > 0x7fff) {
    Mapper::WriteExtendedRAM(address, value);
    return;
  }
  if (control_ & 0x80)
    Mapper::WriteExtendedRAM(address, value);
}

Byte Mapper085::ReadExtendedRAM(Address address) {
  if (address >= 0x6000 && address <= 0x7fff && !(control_ & 0x80))
    return static_cast<Byte>(address >> 8);
  return Mapper::ReadExtendedRAM(address);
}

Byte* Mapper085::GetExtendedRAMPointer() {
  return control_ & 0x80 ? Mapper::GetExtendedRAMPointer() : nullptr;
}

NametableMirroring Mapper085::GetNametableMirroring() {
  constexpr NametableMirroring kMirroringModes[] = {
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
  };
  return kMirroringModes[control_ & 0x03];
}

bool Mapper085::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper085::M2CycleIRQ() {
  ++audio_cycles_;
  if (!irq_enabled_)
    return;

  irq_prescaler_ -= 3;
  if (!irq_cycle_mode_ && irq_prescaler_ > 0)
    return;

  if (irq_counter_ == 0xff) {
    irq_counter_ = irq_reload_;
    if (irq_callback())
      irq_callback().Run();
  } else {
    ++irq_counter_;
  }
  irq_prescaler_ += 341;
}

void Mapper085::SetExpansionAudioOutput(Blip_Buffer* output) {
  audio_.set_output(HasExpansionAudio() ? output : nullptr);
}

void Mapper085::SetExpansionAudioVolume(float volume) {
  audio_.volume(volume);
}

void Mapper085::EndExpansionAudioFrame(int64_t cycles) {
  if (HasExpansionAudio())
    audio_.end_frame(static_cast<blip_time_t>(cycles));
  audio_cycles_ = 0;
}

void Mapper085::Serialize(EmulatorStates::SerializableStateData& data) {
  vrc7_snapshot_t audio_state{};
  audio_.save_snapshot(&audio_state);
  data.WriteData(prg_banks_)
      .WriteData(chr_banks_)
      .WriteData(control_)
      .WriteData(irq_reload_)
      .WriteData(irq_counter_)
      .WriteData(irq_prescaler_)
      .WriteData(irq_enabled_)
      .WriteData(irq_enabled_after_ack_)
      .WriteData(irq_cycle_mode_)
      .WriteData(audio_cycles_)
      .WriteData(audio_state);
  if (!character_ram_.empty())
    data.WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper085::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  vrc7_snapshot_t audio_state{};
  data.ReadData(&prg_banks_)
      .ReadData(&chr_banks_)
      .ReadData(&control_)
      .ReadData(&irq_reload_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_prescaler_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_enabled_after_ack_)
      .ReadData(&irq_cycle_mode_)
      .ReadData(&audio_cycles_)
      .ReadData(&audio_state);
  if (!character_ram_.empty())
    data.ReadData(&character_ram_);
  audio_.load_snapshot(audio_state);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

Address Mapper085::TranslateAddress(Address address) const {
  if ((address & 0xf000) == 0x9000)
    return address & 0xf030;

  Byte selector = 0;
  switch (wiring_) {
    case Wiring::kLegacy:
      selector = static_cast<Byte>(((address >> 3) | (address >> 4)) & 0x01);
      break;
    case Wiring::kA3:
      selector = (address >> 3) & 0x01;
      break;
    case Wiring::kA4:
      selector = (address >> 4) & 0x01;
      break;
  }
  return static_cast<Address>((address & 0xf020) | (selector << 4));
}

size_t Mapper085::GetCHRAddress(Address address) const {
  DCHECK_LT(address, 0x2000);
  const size_t window = (address >> 10) & 0x07;
  const size_t bank = chr_banks_[window] % chr_bank_count_;
  return bank * kCHRBankSize + (address & 0x03ff);
}

bool Mapper085::HasExpansionAudio() const {
  return wiring_ != Wiring::kA3;
}

void Mapper085::SetIRQControl(Byte value) {
  irq_enabled_after_ack_ = (value & 0x01) != 0;
  irq_enabled_ = (value & 0x02) != 0;
  irq_cycle_mode_ = (value & 0x04) != 0;
  if (irq_enabled_) {
    irq_counter_ = irq_reload_;
    irq_prescaler_ = 341;
  }
}

void Mapper085::AcknowledgeIRQ() {
  irq_enabled_ = irq_enabled_after_ack_;
}

}  // namespace nes
}  // namespace kiwi
