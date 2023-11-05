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

#include "nes/mappers/mapper074.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
Mapper074::Mapper074(Cartridge* cartridge) : Mapper004(cartridge) {
  // Mapper 74 has more banks, so the mask will be set to 0xf.
  target_register_mask_ = 0xf;

  // 2KB PRG RAM
  prg_ram_.resize(0x800);
}

Mapper074::~Mapper074() = default;

void Mapper074::WriteCHR(Address address, Byte value) {
  Mapper004::WriteCHR(address, value);
  prg_ram_[address % prg_ram_.size()] = value;
}

void Mapper074::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper004::Serialize(data);
  data.WriteData(prg_ram_);
}

bool Mapper074::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  bool success = Mapper004::Deserialize(header, data);
  if (success)
    data.ReadData(&prg_ram_);

  return true;
}

Byte Mapper074::ReadCHRByBank(int bank, Address address) {
  if (bank == 8 || bank == 9) {
    return prg_ram_.at((bank - 8) * 0x400 + (address % 0x400));
  } else {
    return Mapper004::ReadCHRByBank(bank, address);
  }
}

}  // namespace nes
}  // namespace kiwi
