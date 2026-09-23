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

#include "nes/mappers/mapper024.h"

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

Mapper024::Mapper024(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK(rom_data()->mapper == 24 || rom_data()->mapper == 26);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 4 * kPRGBankSize);

  is_vrc6b_ = rom_data()->mapper == 26;
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
  Reset();
}

Mapper024::~Mapper024() = default;

void Mapper024::Reset() {
  prg_bank_16k_ = 0;
  prg_bank_8k_ = 0;
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  banking_mode_ = 0;
  irq_reload_ = 0;
  irq_counter_ = 0;
  irq_prescaler_ = 0;
  irq_enabled_ = false;
  irq_enabled_after_ack_ = false;
  irq_cycle_mode_ = false;
  audio_.reset();
  audio_cycles_ = 0;
}

void Mapper024::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  const Address reg = TranslateAddress(address) & 0xf003;
  switch (reg) {
    case 0x8000:
    case 0x8001:
    case 0x8002:
    case 0x8003:
      prg_bank_16k_ = value & 0x0f;
      break;
    case 0x9000:
    case 0x9001:
    case 0x9002:
      audio_.write_osc(static_cast<cpu_time_t>(audio_cycles_), 0, reg & 0x03,
                       value);
      break;
    case 0x9003:
      break;
    case 0xa000:
    case 0xa001:
    case 0xa002:
      audio_.write_osc(static_cast<cpu_time_t>(audio_cycles_), 1, reg & 0x03,
                       value);
      break;
    case 0xb000:
    case 0xb001:
    case 0xb002:
      audio_.write_osc(static_cast<cpu_time_t>(audio_cycles_), 2, reg & 0x03,
                       value);
      break;
    case 0xb003:
      if (banking_mode_ != value) {
        banking_mode_ = value;
        if (mirroring_changed_callback())
          mirroring_changed_callback().Run();
      }
      break;
    case 0xc000:
    case 0xc001:
    case 0xc002:
    case 0xc003:
      prg_bank_8k_ = value & 0x1f;
      break;
    case 0xd000:
    case 0xd001:
    case 0xd002:
    case 0xd003:
      chr_banks_[reg & 0x03] = value;
      break;
    case 0xe000:
    case 0xe001:
    case 0xe002:
    case 0xe003:
      chr_banks_[4 + (reg & 0x03)] = value;
      break;
    case 0xf000:
      irq_reload_ = value;
      break;
    case 0xf001:
      SetIRQControl(value);
      break;
    case 0xf002:
      AcknowledgeIRQ();
      break;
  }
}

Byte Mapper024::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);

  size_t bank = 0;
  if (address < 0xc000) {
    bank = static_cast<size_t>(prg_bank_16k_) * 2 +
           ((address - 0x8000) / kPRGBankSize);
  } else if (address < 0xe000) {
    bank = prg_bank_8k_;
  } else {
    bank = prg_bank_count_ - 1;
  }
  bank %= prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x1fff)];
}

void Mapper024::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[GetCHRAddress(address)] = value;
}

Byte Mapper024::ReadCHR(Address address) {
  const size_t index = GetCHRAddress(address);
  return character_ram_.empty() ? rom_data()->CHR[index]
                                : character_ram_[index];
}

uint32_t Mapper024::GetAbsoluteCHRAddress(Address address) {
  return static_cast<uint32_t>(GetCHRAddress(address));
}

void Mapper024::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 || address > 0x7fff) {
    Mapper::WriteExtendedRAM(address, value);
    return;
  }
  if (banking_mode_ & 0x80)
    Mapper::WriteExtendedRAM(address, value);
}

Byte Mapper024::ReadExtendedRAM(Address address) {
  if (address >= 0x6000 && address <= 0x7fff && !(banking_mode_ & 0x80)) {
    return static_cast<Byte>(address >> 8);
  }
  return Mapper::ReadExtendedRAM(address);
}

Byte* Mapper024::GetExtendedRAMPointer() {
  return banking_mode_ & 0x80 ? Mapper::GetExtendedRAMPointer() : nullptr;
}

NametableMirroring Mapper024::GetNametableMirroring() {
  const Byte pages[] = {
      static_cast<Byte>(GetNametableBank(0x2000) & 0x01),
      static_cast<Byte>(GetNametableBank(0x2400) & 0x01),
      static_cast<Byte>(GetNametableBank(0x2800) & 0x01),
      static_cast<Byte>(GetNametableBank(0x2c00) & 0x01),
  };
  if (pages[0] == 0 && pages[1] == 1 && pages[2] == 0 && pages[3] == 1)
    return NametableMirroring::kVertical;
  if (pages[0] == 0 && pages[1] == 0 && pages[2] == 1 && pages[3] == 1)
    return NametableMirroring::kHorizontal;
  if (pages[0] == 0 && pages[1] == 0 && pages[2] == 0 && pages[3] == 0)
    return NametableMirroring::kOneScreenLower;
  if (pages[0] == 1 && pages[1] == 1 && pages[2] == 1 && pages[3] == 1)
    return NametableMirroring::kOneScreenHigher;
  return NametableMirroring::kFourScreen;
}

bool Mapper024::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper024::M2CycleIRQ() {
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

bool Mapper024::UsesCustomPPUMemoryMapping() const {
  return true;
}

Byte Mapper024::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHR(address);

  const Byte bank = GetNametableBank(address);
  const size_t offset = address & 0x03ff;
  if (!(banking_mode_ & 0x10))
    return ciram[(bank & 0x01) * kCHRBankSize + offset];

  const size_t index =
      (static_cast<size_t>(bank) % chr_bank_count_) * kCHRBankSize + offset;
  return character_ram_.empty() ? rom_data()->CHR[index]
                                : character_ram_[index];
}

void Mapper024::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    WriteCHR(address, value);
    return;
  }

  const Byte bank = GetNametableBank(address);
  const size_t offset = address & 0x03ff;
  if (!(banking_mode_ & 0x10)) {
    ciram[(bank & 0x01) * kCHRBankSize + offset] = value;
  } else if (!character_ram_.empty()) {
    const size_t index =
        (static_cast<size_t>(bank) % chr_bank_count_) * kCHRBankSize + offset;
    character_ram_[index] = value;
  }
}

void Mapper024::SetExpansionAudioOutput(Blip_Buffer* output) {
  audio_.output(output);
}

void Mapper024::SetExpansionAudioVolume(float volume) {
  audio_.volume(volume);
}

void Mapper024::EndExpansionAudioFrame(int64_t cycles) {
  audio_.end_frame(static_cast<cpu_time_t>(cycles));
  audio_cycles_ = 0;
}

void Mapper024::Serialize(EmulatorStates::SerializableStateData& data) {
  vrc6_snapshot_t audio_state{};
  audio_.save_snapshot(&audio_state);
  data.WriteData(prg_bank_16k_)
      .WriteData(prg_bank_8k_)
      .WriteData(chr_banks_)
      .WriteData(banking_mode_)
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

bool Mapper024::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  vrc6_snapshot_t audio_state{};
  data.ReadData(&prg_bank_16k_)
      .ReadData(&prg_bank_8k_)
      .ReadData(&chr_banks_)
      .ReadData(&banking_mode_)
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

Address Mapper024::TranslateAddress(Address address) const {
  if (!is_vrc6b_)
    return address;
  return static_cast<Address>((address & 0xfffc) | ((address & 0x01) << 1) |
                              ((address & 0x02) >> 1));
}

size_t Mapper024::GetCHRAddress(Address address) const {
  DCHECK_LT(address, 0x2000);
  const size_t window = address >> 10;
  const Byte mode = banking_mode_ & 0x03;
  Byte bank = 0;
  if (mode == 0) {
    bank = chr_banks_[window];
  } else if (mode == 1) {
    bank = chr_banks_[window >> 1];
    if (banking_mode_ & 0x20)
      bank = static_cast<Byte>((bank & 0xfe) | (window & 0x01));
  } else if (window < 4) {
    bank = chr_banks_[window];
  } else {
    bank = chr_banks_[4 + ((window - 4) >> 1)];
    if (banking_mode_ & 0x20)
      bank = static_cast<Byte>((bank & 0xfe) | (window & 0x01));
  }
  return (static_cast<size_t>(bank) % chr_bank_count_) * kCHRBankSize +
         (address & 0x03ff);
}

Byte Mapper024::GetNametableBank(Address address) const {
  DCHECK_GE(address, 0x2000);
  DCHECK_LT(address, 0x3000);
  const Byte window = (address >> 10) & 0x03;
  const Byte mode = banking_mode_ & 0x2f;

  if (banking_mode_ & 0x20) {
    switch (mode) {
      case 0x20:
      case 0x27:
        return static_cast<Byte>((chr_banks_[6 + (window >> 1)] & 0xfe) |
                                 (window & 0x01));
      case 0x23:
      case 0x24:
        return static_cast<Byte>((chr_banks_[6 + (window & 0x01)] & 0xfe) |
                                 (window >> 1));
      case 0x28:
      case 0x2f:
        return chr_banks_[6 + (window >> 1)] & 0xfe;
      case 0x2b:
      case 0x2c:
        return static_cast<Byte>((chr_banks_[6 + (window & 0x01)] & 0xfe) |
                                 0x01);
    }
  }

  switch (mode & 0x07) {
    case 0:
    case 6:
    case 7:
      return chr_banks_[6 + (window >> 1)];
    case 1:
    case 5:
      return chr_banks_[4 + window];
    default:
      return chr_banks_[6 + (window & 0x01)];
  }
}

void Mapper024::SetIRQControl(Byte value) {
  irq_enabled_after_ack_ = (value & 0x01) != 0;
  irq_enabled_ = (value & 0x02) != 0;
  irq_cycle_mode_ = (value & 0x04) != 0;
  if (irq_enabled_) {
    irq_counter_ = irq_reload_;
    irq_prescaler_ = 341;
  }
}

void Mapper024::AcknowledgeIRQ() {
  irq_enabled_ = irq_enabled_after_ack_;
}

}  // namespace nes
}  // namespace kiwi
