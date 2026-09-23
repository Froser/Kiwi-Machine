// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper019.h"

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
constexpr size_t kCIRAMSize = 0x0800;
constexpr size_t kAudioRAMSize = 0x0080;

}  // namespace

Mapper019::Mapper019(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK(rom_data()->mapper == 19 || rom_data()->mapper == 210);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);

  if (rom_data()->mapper == 210) {
    if (rom_data()->submapper == 1) {
      variant_ = Variant::kNamco175;
    } else if (rom_data()->submapper == 2) {
      variant_ = Variant::kNamco340;
    } else {
      variant_ =
          rom_data()->has_battery ? Variant::kNamco175 : Variant::kNamco340;
    }
  }

  external_prg_nvram_size_ = rom_data()->prg_nvram_size;
  size_t external_prg_ram_size =
      rom_data()->prg_ram_size + external_prg_nvram_size_;
  if (variant_ == Variant::kNamco340) {
    external_prg_nvram_size_ = 0;
    external_prg_ram_size = 0;
  }
  prg_ram_.resize(external_prg_ram_size);

  // The N163's internal 128 bytes are battery-backed independently from the
  // optional external 8 KiB RAM and are excluded from NES 2.0 RAM fields.
  if (variant_ == Variant::kNamco163 && rom_data()->has_battery)
    rom_data()->prg_nvram_size += kAudioRAMSize;

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = character_ram_.size() / kCHRBankSize;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  ResetRegisters();
}

Mapper019::~Mapper019() = default;

void Mapper019::Reset() {
  ResetRegisters();
}

void Mapper019::WritePRG(Address address, Byte value) {
  const Address reg = address & 0xf800;
  if (reg >= 0x8000 && reg <= 0xb800) {
    chr_banks_[(reg - 0x8000) >> 11] = value;
    return;
  }

  if (reg >= 0xc000 && reg <= 0xd800) {
    if (variant_ == Variant::kNamco163) {
      nametable_banks_[(reg - 0xc000) >> 11] = value;
    } else if (variant_ == Variant::kNamco175 && reg == 0xc000) {
      prg_ram_enabled_ = (value & 0x01) != 0;
    }
    return;
  }

  switch (reg) {
    case 0xe000:
      prg_banks_[0] = value & 0x3f;
      if (variant_ == Variant::kNamco163) {
        audio_.set_enabled(static_cast<cpu_time_t>(audio_cycles_),
                           (value & 0x40) == 0);
      } else if (variant_ == Variant::kNamco340) {
        constexpr NametableMirroring kMirroringModes[] = {
            NametableMirroring::kOneScreenLower,
            NametableMirroring::kVertical,
            NametableMirroring::kOneScreenHigher,
            NametableMirroring::kHorizontal,
        };
        const NametableMirroring mirroring = kMirroringModes[value >> 6];
        if (mirroring_ != mirroring) {
          mirroring_ = mirroring;
          if (mirroring_changed_callback())
            mirroring_changed_callback().Run();
        }
      }
      break;
    case 0xe800:
      prg_banks_[1] = value & 0x3f;
      if (variant_ == Variant::kNamco163) {
        low_chr_ciram_disabled_ = (value & 0x40) != 0;
        high_chr_ciram_disabled_ = (value & 0x80) != 0;
      }
      break;
    case 0xf000:
      prg_banks_[2] = value & 0x3f;
      break;
    case 0xf800:
      if (variant_ == Variant::kNamco163) {
        write_protect_ = value;
        audio_.write_addr(value);
      }
      break;
  }
}

Byte Mapper019::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t window = (address - 0x8000) / kPRGBankSize;
  const Byte bank =
      window < 3 ? prg_banks_[window]
                 : static_cast<Byte>(rom_data()->PRG.size() / kPRGBankSize - 1);
  return ReadPRGBank(bank, address);
}

void Mapper019::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty() &&
      !MapsCIRAM((address >> 10) & 0x07, chr_banks_[address >> 10])) {
    character_ram_[GetCHRAddress(address)] = value;
  }
}

Byte Mapper019::ReadCHR(Address address) {
  if (MapsCIRAM((address >> 10) & 0x07, chr_banks_[address >> 10]))
    return 0;
  const size_t index = GetCHRAddress(address);
  return character_ram_.empty() ? rom_data()->CHR[index]
                                : character_ram_[index];
}

uint32_t Mapper019::GetAbsoluteCHRAddress(Address address) {
  if (MapsCIRAM((address >> 10) & 0x07, chr_banks_[address >> 10]))
    return address & 0x07ff;
  return static_cast<uint32_t>(GetCHRAddress(address));
}

void Mapper019::WriteExtendedRAM(Address address, Byte value) {
  if (variant_ == Variant::kNamco163) {
    switch (address & 0xf800) {
      case 0x4800: {
        const bool changed = audio_.peek_data() != value;
        audio_.write_data(static_cast<cpu_time_t>(audio_cycles_), value);
        if (changed && rom_data()->has_battery)
          MarkPRGNVRAMDirty();
        return;
      }
      case 0x5000:
        irq_counter_ = static_cast<uint16_t>((irq_counter_ & 0xff00) | value);
        return;
      case 0x5800:
        irq_counter_ =
            static_cast<uint16_t>((irq_counter_ & 0x00ff) | (value << 8));
        return;
    }
  }

  if (address < 0x6000 || address > 0x7fff || prg_ram_.empty() ||
      !CanWritePRGRAM(address)) {
    return;
  }

  const size_t index = (address - 0x6000) % prg_ram_.size();
  if (prg_ram_[index] != value) {
    prg_ram_[index] = value;
    if (index < external_prg_nvram_size_)
      MarkPRGNVRAMDirty();
  }
}

Byte Mapper019::ReadExtendedRAM(Address address) {
  if (variant_ == Variant::kNamco163) {
    switch (address & 0xf800) {
      case 0x4800:
        return static_cast<Byte>(audio_.read_data());
      case 0x5000:
        return irq_counter_ & 0xff;
      case 0x5800:
        return irq_counter_ >> 8;
    }
  }

  if (address >= 0x6000 && address <= 0x7fff && !prg_ram_.empty())
    return prg_ram_[(address - 0x6000) % prg_ram_.size()];
  return Mapper::ReadExtendedRAM(address);
}

Byte* Mapper019::GetExtendedRAMPointer() {
  return prg_ram_.size() >= 0x2000 ? prg_ram_.data() : nullptr;
}

bool Mapper019::UsesCustomPRGRAM() const {
  return true;
}

NametableMirroring Mapper019::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper019::NeedsM2CycleIRQ() const {
  return variant_ == Variant::kNamco163;
}

void Mapper019::M2CycleIRQ() {
  ++audio_cycles_;
  if ((irq_counter_ & 0x8000) == 0 || (irq_counter_ & 0x7fff) == 0x7fff)
    return;

  ++irq_counter_;
  if ((irq_counter_ & 0x7fff) == 0x7fff && irq_callback())
    irq_callback().Run();
}

bool Mapper019::UsesCustomPPUMemoryMapping() const {
  return variant_ == Variant::kNamco163;
}

Byte Mapper019::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  const size_t window = address >> 10;
  const Byte bank =
      window < 8 ? chr_banks_[window] : nametable_banks_[window - 8];
  if (MapsCIRAM(window, bank))
    return ciram[(bank & 0x01) * kCHRBankSize + (address & 0x03ff)];

  const size_t index =
      (static_cast<size_t>(bank) % chr_bank_count_) * kCHRBankSize +
      (address & 0x03ff);
  return character_ram_.empty() ? rom_data()->CHR[index]
                                : character_ram_[index];
}

void Mapper019::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  const size_t window = address >> 10;
  const Byte bank =
      window < 8 ? chr_banks_[window] : nametable_banks_[window - 8];
  if (MapsCIRAM(window, bank)) {
    ciram[(bank & 0x01) * kCHRBankSize + (address & 0x03ff)] = value;
  } else if (!character_ram_.empty()) {
    const size_t index =
        (static_cast<size_t>(bank) % chr_bank_count_) * kCHRBankSize +
        (address & 0x03ff);
    character_ram_[index] = value;
  }
}

void Mapper019::SetExpansionAudioOutput(Blip_Buffer* output) {
  audio_.output(HasExpansionAudio() ? output : nullptr);
}

void Mapper019::SetExpansionAudioVolume(float volume) {
  float gain = 1.f;
  switch (rom_data()->submapper) {
    case 4:
      gain = 1.68f;
      break;
    case 5:
      gain = 2.18f;
      break;
  }
  audio_.volume(volume * gain);
}

void Mapper019::EndExpansionAudioFrame(int64_t cycles) {
  if (HasExpansionAudio())
    audio_.end_frame(static_cast<cpu_time_t>(cycles));
  audio_cycles_ = 0;
}

void Mapper019::Serialize(EmulatorStates::SerializableStateData& data) {
  namco_snapshot_t audio_state{};
  audio_.save_snapshot(&audio_state);
  data.WriteData(prg_banks_)
      .WriteData(chr_banks_)
      .WriteData(nametable_banks_)
      .WriteData(write_protect_)
      .WriteData(irq_counter_)
      .WriteData(prg_ram_enabled_)
      .WriteData(low_chr_ciram_disabled_)
      .WriteData(high_chr_ciram_disabled_)
      .WriteData(mirroring_)
      .WriteData(audio_cycles_)
      .WriteData(audio_state)
      .WriteData(prg_ram_);
  if (!character_ram_.empty())
    data.WriteData(character_ram_);
}

bool Mapper019::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  Bytes previous_nvram;
  if (HasBatteryBackedRAM())
    previous_nvram = CopyPRGNVRAM();

  namco_snapshot_t audio_state{};
  data.ReadData(&prg_banks_)
      .ReadData(&chr_banks_)
      .ReadData(&nametable_banks_)
      .ReadData(&write_protect_)
      .ReadData(&irq_counter_)
      .ReadData(&prg_ram_enabled_)
      .ReadData(&low_chr_ciram_disabled_)
      .ReadData(&high_chr_ciram_disabled_)
      .ReadData(&mirroring_)
      .ReadData(&audio_cycles_)
      .ReadData(&audio_state)
      .ReadData(&prg_ram_);
  if (!character_ram_.empty())
    data.ReadData(&character_ram_);
  audio_.load_snapshot(audio_state);

  if (HasBatteryBackedRAM() && previous_nvram != CopyPRGNVRAM())
    MarkPRGNVRAMDirty();
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return true;
}

Bytes Mapper019::CopyPRGNVRAM() {
  if (!HasBatteryBackedRAM())
    return {};

  Bytes data;
  data.reserve(GetPRGNVRAMSize());
  data.insert(data.end(), prg_ram_.begin(),
              prg_ram_.begin() + external_prg_nvram_size_);
  if (variant_ == Variant::kNamco163 && rom_data()->has_battery) {
    data.insert(data.end(), audio_.ram(), audio_.ram() + kAudioRAMSize);
  }
  return data;
}

bool Mapper019::RestorePRGNVRAM(const Bytes& data) {
  if (data.size() != GetPRGNVRAMSize())
    return false;

  std::copy(data.begin(), data.begin() + external_prg_nvram_size_,
            prg_ram_.begin());
  if (variant_ == Variant::kNamco163 && rom_data()->has_battery)
    audio_.load_ram(data.data() + external_prg_nvram_size_);
  return true;
}

void Mapper019::ResetRegisters() {
  std::fill(std::begin(prg_banks_), std::end(prg_banks_), 0);
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  if (rom_data()->name_table_mirroring == NametableMirroring::kVertical) {
    nametable_banks_[0] = nametable_banks_[2] = 0xe0;
    nametable_banks_[1] = nametable_banks_[3] = 0xe1;
  } else {
    nametable_banks_[0] = nametable_banks_[1] = 0xe0;
    nametable_banks_[2] = nametable_banks_[3] = 0xe1;
  }
  write_protect_ = 0;
  irq_counter_ = 0;
  prg_ram_enabled_ = false;
  low_chr_ciram_disabled_ = false;
  high_chr_ciram_disabled_ = false;
  mirroring_ = rom_data()->name_table_mirroring;
  audio_cycles_ = 0;
  audio_.reset(false);
}

bool Mapper019::MapsCIRAM(size_t window, Byte bank) const {
  if (variant_ != Variant::kNamco163 || bank < 0xe0)
    return false;
  if (window >= 8)
    return true;
  return window < 4 ? !low_chr_ciram_disabled_ : !high_chr_ciram_disabled_;
}

size_t Mapper019::GetCHRAddress(Address address) const {
  const Byte bank = chr_banks_[(address >> 10) & 0x07];
  return (static_cast<size_t>(bank) % chr_bank_count_) * kCHRBankSize +
         (address & 0x03ff);
}

Byte Mapper019::ReadPRGBank(Byte bank, Address address) {
  const size_t bank_count = rom_data()->PRG.size() / kPRGBankSize;
  const size_t index = (static_cast<size_t>(bank) % bank_count) * kPRGBankSize +
                       (address & 0x1fff);
  return rom_data()->PRG[index];
}

bool Mapper019::CanWritePRGRAM(Address address) const {
  if (variant_ == Variant::kNamco175)
    return prg_ram_enabled_;
  if (variant_ != Variant::kNamco163 || (write_protect_ & 0xf0) != 0x40)
    return false;
  return (write_protect_ & (1 << ((address - 0x6000) >> 11))) == 0;
}

bool Mapper019::HasExpansionAudio() {
  if (variant_ != Variant::kNamco163)
    return false;
  return rom_data()->submapper == 0 || rom_data()->submapper >= 3;
}

}  // namespace nes
}  // namespace kiwi
