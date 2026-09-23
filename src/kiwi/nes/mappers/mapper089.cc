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

#include "nes/mappers/mapper089.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper089::Mapper089(Cartridge* cartridge)
    : Mapper(cartridge), mirroring_(rom_data()->name_table_mirroring) {
  DCHECK_EQ(rom_data()->mapper, 89);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper089::~Mapper089() = default;

void Mapper089::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper089::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  value &= ReadPRG(address);
  selected_prg_bank_ = (value >> 4) & 0x07;
  selected_chr_bank_ = (value & 0x07) | ((value & 0x80) >> 4);

  const NametableMirroring mirroring =
      value & 0x08 ? NametableMirroring::kOneScreenHigher
                   : NametableMirroring::kOneScreenLower;
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

Byte Mapper089::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = address < 0xc000 ? selected_prg_bank_ % prg_bank_count_
                                       : prg_bank_count_ - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper089::WriteCHR(Address address, Byte value) {}

Byte Mapper089::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper089::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

NametableMirroring Mapper089::GetNametableMirroring() {
  return mirroring_;
}

void Mapper089::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(mirroring_);
  Mapper::Serialize(data);
}

bool Mapper089::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&mirroring_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
