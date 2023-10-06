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

#include "nes/mappers/mapper087.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

// No bus conflicts, no WRAM. Register addresses are from 0x6000 to 0xffff.
Mapper087::Mapper087(Cartridge* cartridge) : Mapper(cartridge) {}

Mapper087::~Mapper087() = default;

void Mapper087::WritePRG(Address address, Byte value) {
  if (address >= 0x6000) {
    select_chr_ = ((value >> 1) & 1) | ((value & 1) << 1);
  } else {
    LOG(ERROR) << "Can't write value $" << Hex<16>{value} << " to PRG address $"
               << Hex<16>{address} << ", because it is read only.";
  }
}

Byte Mapper087::ReadPRG(Address address) {
  if (address >= 0x8000) {
    return cartridge()->GetRomData()->PRG[address - 0x8000];
  } else {
    CHECK(false) << "Shouldn't happen.";
    return 0;
  }
}

void Mapper087::WriteCHR(Address address, Byte value) {
  LOG(ERROR) << "CHR read-only.";
}

Byte Mapper087::ReadCHR(Address address) {
  Address base_address = select_chr_ * 0x2000;
  return cartridge()->GetRomData()->CHR[base_address + address];
}

void Mapper087::Serialize(EmulatorStates::SerializableStateData& data) {}

bool Mapper087::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {}
}  // namespace nes
}  // namespace kiwi
