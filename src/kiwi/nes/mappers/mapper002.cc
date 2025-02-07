// Copyright (C) 2023 Yisi Yu
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

#include "nes/mappers/mapper002.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
Mapper002::Mapper002(Cartridge* cartridge) : Mapper(cartridge) {
  if (cartridge->GetRomData()->CHR.size() == 0) {
    uses_character_ram_ = true;
    character_ram_.resize(0x2000);
  } else {
    uses_character_ram_ = false;
  }

  last_bank_offset_ = cartridge->GetRomData()->PRG.size() - 0x4000;
}

Mapper002::~Mapper002() = default;

// 7  bit  0
// ---- ----
// xxxx pPPP
//      ||||
//      ++++- Select 16 KB PRG ROM bank for CPU $8000-$BFFF
//           (UNROM uses bits 2-0; UOROM uses bits 3-0)
void Mapper002::WritePRG(Address address, Byte value) {
  select_prg_ = value;
}

Byte Mapper002::ReadPRG(Address address) {
  if (address < 0xc000)
    return rom_data()->PRG[((address - 0x8000) & 0x3fff) | (select_prg_ << 14)];

  return *(last_bank() + (address & 0x3fff));
}

void Mapper002::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_) {
    character_ram_[address] = value;
  }
}

Byte Mapper002::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address];
  else
    return rom_data()->CHR[address];
}

void Mapper002::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(select_prg_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);

  Mapper::Serialize(data);
}

bool Mapper002::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&select_prg_);
  if (uses_character_ram_) {
    CHECK(character_ram_.size() == 0x2000);
    data.ReadData(&character_ram_);
  }
  return Mapper::Deserialize(header, data);
}

Byte* Mapper002::last_bank() {
  return rom_data()->PRG.data() + last_bank_offset_;
}

}  // namespace nes
}  // namespace kiwi
