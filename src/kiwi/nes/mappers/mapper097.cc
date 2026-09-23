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

#include "nes/mappers/mapper097.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;

}  // namespace

Mapper097::Mapper097(Cartridge* cartridge)
    : Mapper(cartridge), mirroring_(rom_data()->name_table_mirroring) {
  DCHECK_EQ(rom_data()->mapper, 97);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
  DCHECK(rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  character_ram_.resize(kCHRRAMSize);
  Reset();
}

Mapper097::~Mapper097() = default;

void Mapper097::Reset() {
  selected_prg_bank_ = static_cast<Byte>(prg_bank_count_ - 1);
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper097::WritePRG(Address address, Byte value) {
  if (address < 0x8000 || address >= 0xc000)
    return;

  selected_prg_bank_ = value & 0x0f;
  const NametableMirroring mirroring = value & 0x80
                                           ? NametableMirroring::kVertical
                                           : NametableMirroring::kHorizontal;
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

Byte Mapper097::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = address < 0xc000 ? prg_bank_count_ - 1
                                       : selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper097::WriteCHR(Address address, Byte value) {
  DCHECK_LT(address, 0x2000);
  character_ram_[address] = value;
}

Byte Mapper097::ReadCHR(Address address) {
  DCHECK_LT(address, 0x2000);
  return character_ram_[address];
}

NametableMirroring Mapper097::GetNametableMirroring() {
  return mirroring_;
}

void Mapper097::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper097::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&mirroring_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
