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

#include "nes/mappers/mapper156.h"

#include <algorithm>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kWorkRAMSize = 0x2000;

}  // namespace

Mapper156::Mapper156(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 156);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  EnsurePRGRAM(kWorkRAMSize);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper156::~Mapper156() = default;

void Mapper156::Reset() {
  selected_prg_bank_ = 0;
  std::fill(std::begin(chr_low_), std::end(chr_low_), 0);
  std::fill(std::begin(chr_high_), std::end(chr_high_), 0);
  SetMirroring(2);
}

void Mapper156::WritePRG(Address address, Byte value) {
  if (address >= 0xc000 && address <= 0xc00f) {
    const size_t bank = (address & 0x03) | ((address & 0x08) ? 4 : 0);
    if (address & 0x04) {
      chr_high_[bank] = value & 0x01;
    } else {
      chr_low_[bank] = value;
    }
  } else if (address == 0xc010) {
    selected_prg_bank_ = value;
  } else if (address == 0xc014) {
    SetMirroring(value);
  }
}

Byte Mapper156::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = address < 0xc000 ? selected_prg_bank_ % prg_bank_count_
                                       : prg_bank_count_ - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper156::WriteCHR(Address address, Byte value) {}

Byte Mapper156::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper156::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t slot = address / kCHRBankSize;
  const size_t selected_bank =
      (static_cast<size_t>(chr_high_[slot]) << 8) | chr_low_[slot];
  const size_t bank = selected_bank % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize +
                               (address & (kCHRBankSize - 1)));
}

NametableMirroring Mapper156::GetNametableMirroring() {
  return mirroring_;
}

void Mapper156::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(chr_low_)
      .WriteData(chr_high_)
      .WriteData(mirroring_);
  Mapper::Serialize(data);
}

bool Mapper156::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&chr_low_)
      .ReadData(&chr_high_)
      .ReadData(&mirroring_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper156::SetMirroring(Byte value) {
  NametableMirroring mirroring = NametableMirroring::kOneScreenLower;
  if ((value & 0x03) == 0) {
    mirroring = NametableMirroring::kVertical;
  } else if ((value & 0x03) == 1) {
    mirroring = NametableMirroring::kHorizontal;
  }

  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

}  // namespace nes
}  // namespace kiwi
