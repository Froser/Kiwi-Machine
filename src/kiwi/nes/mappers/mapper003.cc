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

#include "nes/mappers/mapper003.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
Mapper003::Mapper003(Cartridge* cartridge) : Mapper(cartridge) {
  if (cartridge->GetRomData()->PRG.size() == 0x4000) {
    is_one_bank_ = true;
  } else {
    is_one_bank_ = false;
  }
}

Mapper003::~Mapper003() = default;

// 7  bit  0
//---- ----
// xxDD xxCC
//   ||   ||
//   ||   ++- Select 8 KB CHR ROM bank for PPU $0000-$1FFF
//   ++------ Security diodes config
void Mapper003::WritePRG(Address address, Byte value) {
  select_chr_ = value & 0x3;
}

Byte Mapper003::ReadPRG(Address address) {
  if (!is_one_bank_)
    return cartridge()->GetRomData()->PRG[address - 0x8000];

  return cartridge()->GetRomData()->PRG[(address - 0x8000) & 0x3fff];
}

void Mapper003::WriteCHR(Address address, Byte value) {
  LOG(ERROR) << "CHR read-only.";
}

Byte Mapper003::ReadCHR(Address address) {
  return cartridge()->GetRomData()->CHR[address | (select_chr_ << 13)];
}

void Mapper003::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(select_chr_);
  Mapper::Serialize(data);
}

bool Mapper003::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&select_chr_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
