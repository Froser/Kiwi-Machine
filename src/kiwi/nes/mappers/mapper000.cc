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

#include "nes/mappers/mapper000.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
Mapper000::Mapper000(Cartridge* cartridge) : Mapper(cartridge) {
  if (cartridge->GetRomData()->PRG.size() == 0x4000) {
    is_one_bank_ = true;
  } else {
    is_one_bank_ = false;
  }

  if (cartridge->GetRomData()->CHR.size() == 0) {
    uses_character_ram_ = true;
    character_ram_.resize(0x2000);
  } else {
    uses_character_ram_ = false;
  }
}

Mapper000::~Mapper000() = default;

void Mapper000::WritePRG(Address address, Byte value) {
  LOG(ERROR) << "Can't write value $" << Hex<16>{value} << " to PRG address $"
             << Hex<16>{address} << ", because it is read only.";
}

// CPU $6000-$7FFF: Family Basic only: PRG RAM, mirrored as necessary to fill
// entire 8 KiB window, write protectable with an external switch
// CPU $8000-$BFFF: First 16 KB of ROM.
// CPU $C000-$FFFF: Last 16 KB of ROM (NROM-256) or mirror of $8000-$BFFF
// (NROM-128).
Byte Mapper000::ReadPRG(Address address) {
  if (!is_one_bank_)
    return rom_data()->PRG[address - 0x8000];
  else
    return rom_data()->PRG[(address - 0x8000) & 0x3fff];
}

void Mapper000::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address] = value;
}

Byte Mapper000::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address];

  return rom_data()->CHR[address];
}

void Mapper000::Serialize(EmulatorStates::SerializableStateData& data) {
  if (uses_character_ram_)
    data.WriteData(character_ram_);

  Mapper::Serialize(data);
}

bool Mapper000::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (uses_character_ram_) {
    CHECK(character_ram_.size() == 0x2000);
    data.ReadData(&character_ram_);
  }

  return Mapper::Deserialize(header, data);
}
}  // namespace nes
}  // namespace kiwi
