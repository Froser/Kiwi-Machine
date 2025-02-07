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

#include "nes/mappers/mapper007.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

// No bus conflicts, no WRAM. Register addresses are from 0x6000 to 0xffff.
Mapper007::Mapper007(Cartridge* cartridge) : Mapper(cartridge) {
  if (cartridge->GetRomData()->CHR.size() == 0) {
    uses_character_ram_ = true;
    character_ram_.resize(0x2000);
  } else {
    uses_character_ram_ = false;
  }
}

Mapper007::~Mapper007() = default;

// 7  bit  0
// ---- ----
// xxxM xPPP
//    |  |||
//    |  +++- Select 32 KB PRG ROM bank for CPU $8000-$FFFF
//    +------ Select 1 KB VRAM page for all 4 nametables
void Mapper007::WritePRG(Address address, Byte value) {
  if (address >= 0x6000) {
    select_prg_ = value & 7;
    select_mirror_ = (value >> 4) & 1;
    mirroring_changed_callback().Run();
  } else {
    LOG(ERROR) << "Can't write value $" << Hex<16>{value} << " to PRG address $"
               << Hex<16>{address} << ", because it is read only.";
  }
}

Byte Mapper007::ReadPRG(Address address) {
  if (address >= 0x8000) {
    return rom_data()->PRG.at(select_prg_ * 0x8000 +
                                             (address - 0x8000));
  } else {
    CHECK(false) << "Shouldn't happen.";
    return 0;
  }
}

void Mapper007::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address] = value;
}

Byte Mapper007::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address];

  return rom_data()->CHR[address];
}

NametableMirroring Mapper007::GetNametableMirroring() {
  return static_cast<NametableMirroring>(
      static_cast<int>(NametableMirroring::kOneScreenLower) + select_mirror_);
}

void Mapper007::Serialize(EmulatorStates::SerializableStateData& data) {
  if (uses_character_ram_)
    data.WriteData(character_ram_);

  data.WriteData(select_prg_).WriteData(select_mirror_);
  Mapper::Serialize(data);
}

bool Mapper007::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (uses_character_ram_) {
    CHECK(character_ram_.size() == 0x2000);
    data.ReadData(&character_ram_);
  }

  data.ReadData(&select_prg_).ReadData(&select_mirror_);
  mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}
}  // namespace nes
}  // namespace kiwi
