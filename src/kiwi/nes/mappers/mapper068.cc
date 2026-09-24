// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper068.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x0800;
constexpr size_t kNametableBankSize = 0x0400;
constexpr size_t kInternalPRGBankCount = 8;
constexpr uint32_t kLicensingTimerCycles = 1024 * 105;

}  // namespace

Mapper068::Mapper068(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 68);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kNametableBankSize, 0u);
  DCHECK_GE(rom_data()->CHR.size(), kCHRBankSize);

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kNametableBankSize;
  has_external_rom_ = prg_bank_count_ > kInternalPRGBankCount;
  Reset();
}

Mapper068::~Mapper068() = default;

void Mapper068::Reset() {
  prg_bank_ = 0;
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  std::fill(std::begin(nametable_banks_), std::end(nametable_banks_), 0x80);
  mirroring_ = rom_data()->name_table_mirroring;
  use_chr_nametables_ = false;
  prg_ram_enabled_ = false;
  using_external_rom_ = false;
  licensing_timer_ = 0;
}

void Mapper068::WritePRG(Address address, Byte value) {
  switch (address & 0xf000) {
    case 0x8000:
    case 0x9000:
    case 0xa000:
    case 0xb000:
      chr_banks_[(address - 0x8000) >> 12] = value;
      break;
    case 0xc000:
      nametable_banks_[0] = value | 0x80;
      break;
    case 0xd000:
      nametable_banks_[1] = value | 0x80;
      break;
    case 0xe000: {
      constexpr NametableMirroring kMirroringModes[] = {
          NametableMirroring::kVertical,
          NametableMirroring::kHorizontal,
          NametableMirroring::kOneScreenLower,
          NametableMirroring::kOneScreenHigher,
      };
      const NametableMirroring mirroring = kMirroringModes[value & 0x03];
      if (mirroring_ != mirroring) {
        mirroring_ = mirroring;
        if (mirroring_changed_callback())
          mirroring_changed_callback().Run();
      }
      use_chr_nametables_ = (value & 0x10) != 0;
      break;
    }
    case 0xf000:
      prg_ram_enabled_ = (value & 0x10) != 0;
      if (has_external_rom_ && !(value & 0x08)) {
        using_external_rom_ = true;
        const size_t external_bank_count =
            prg_bank_count_ - kInternalPRGBankCount;
        prg_bank_ = static_cast<Byte>(kInternalPRGBankCount +
                                      (value & 0x07) % external_bank_count);
      } else {
        using_external_rom_ = false;
        const Byte bank_mask = has_external_rom_ ? 0x07 : 0x0f;
        prg_bank_ = static_cast<Byte>((value & bank_mask) % prg_bank_count_);
      }
      break;
  }
}

Byte Mapper068::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  if (address < 0xc000)
    return ReadSelectedPRGBank(address);

  const size_t internal_bank_count =
      std::min(prg_bank_count_, kInternalPRGBankCount);
  const size_t bank = internal_bank_count - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper068::WriteCHR(Address address, Byte value) {}

Byte Mapper068::ReadCHR(Address address) {
  return rom_data()->CHR[GetCHRAddress(address)];
}

uint32_t Mapper068::GetAbsoluteCHRAddress(Address address) {
  return static_cast<uint32_t>(GetCHRAddress(address));
}

void Mapper068::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 || address > 0x7fff) {
    Mapper::WriteExtendedRAM(address, value);
    return;
  }

  if (prg_ram_enabled_) {
    Mapper::WriteExtendedRAM(address, value);
  } else if (has_external_rom_) {
    licensing_timer_ = kLicensingTimerCycles;
  }
}

Byte Mapper068::ReadExtendedRAM(Address address) {
  if (address >= 0x6000 && address <= 0x7fff) {
    if (prg_ram_enabled_)
      return Mapper::ReadExtendedRAM(address);
    return static_cast<Byte>(address >> 8);
  }
  return Mapper::ReadExtendedRAM(address);
}

NametableMirroring Mapper068::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper068::UsesCustomPPUMemoryMapping() const {
  return true;
}

Byte Mapper068::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHR(address);

  const Byte page = GetNametablePage(address);
  const size_t offset = address & 0x03ff;
  if (use_chr_nametables_) {
    const size_t bank = nametable_banks_[page] % chr_bank_count_;
    return rom_data()->CHR[bank * kNametableBankSize + offset];
  }
  return ciram[page * kNametableBankSize + offset];
}

void Mapper068::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    WriteCHR(address, value);
    return;
  }

  if (!use_chr_nametables_) {
    const Byte page = GetNametablePage(address);
    ciram[page * kNametableBankSize + (address & 0x03ff)] = value;
  }
}

bool Mapper068::NeedsM2CycleIRQ() const {
  return has_external_rom_;
}

void Mapper068::M2CycleIRQ() {
  if (licensing_timer_)
    --licensing_timer_;
}

void Mapper068::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_bank_)
      .WriteData(chr_banks_)
      .WriteData(nametable_banks_)
      .WriteData(mirroring_)
      .WriteData(use_chr_nametables_)
      .WriteData(prg_ram_enabled_)
      .WriteData(using_external_rom_)
      .WriteData(licensing_timer_);
  Mapper::Serialize(data);
}

bool Mapper068::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_bank_)
      .ReadData(&chr_banks_)
      .ReadData(&nametable_banks_)
      .ReadData(&mirroring_)
      .ReadData(&use_chr_nametables_)
      .ReadData(&prg_ram_enabled_)
      .ReadData(&using_external_rom_)
      .ReadData(&licensing_timer_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

Byte Mapper068::GetNametablePage(Address address) const {
  const Byte nametable = (address >> 10) & 0x03;
  switch (mirroring_) {
    case NametableMirroring::kVertical:
      return nametable & 0x01;
    case NametableMirroring::kHorizontal:
      return nametable >> 1;
    case NametableMirroring::kOneScreenHigher:
      return 1;
    default:
      return 0;
  }
}

size_t Mapper068::GetCHRAddress(Address address) const {
  const size_t window = (address >> 11) & 0x03;
  const size_t bank = chr_banks_[window] % (chr_bank_count_ / 2);
  return bank * kCHRBankSize + (address & 0x07ff);
}

Byte Mapper068::ReadSelectedPRGBank(Address address) {
  if (using_external_rom_ && licensing_timer_ == 0)
    return static_cast<Byte>(address >> 8);

  const size_t bank = prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

}  // namespace nes
}  // namespace kiwi
