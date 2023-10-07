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

#include "nes/mappers/mapper066.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

// Bank select:
// 7  bit  0
//---- ----
// xxPP xxCC
//   ||   ||
//   ||   ++- Select 8 KB CHR ROM bank for PPU $0000-$1FFF
//   ++------ Select 32 KB PRG ROM bank for CPU $8000-$FFFF
constexpr int kCHRBankSize = 0x2000;
constexpr int kPRGBankSize = 0x8000;

Mapper066::Mapper066(Cartridge* cartridge) : Mapper(cartridge) {}

Mapper066::~Mapper066() = default;

void Mapper066::WritePRG(Address address, Byte value) {
  if (address >= 0x8000) {
    select_chr_prg_ = value;
  }
}

Byte Mapper066::ReadPRG(Address address) {
  if (address >= 0x8000) {
    int prg_bank = (select_chr_prg_ >> 4);
    uint32_t base_address = kPRGBankSize * prg_bank;
    return cartridge()->GetRomData()->PRG[base_address + address - 0x8000];
  } else {
    CHECK(false) << "Shouldn't happen.";
    return 0;
  }
}

void Mapper066::WriteCHR(Address address, Byte value) {
  LOG(ERROR) << "CHR read-only.";
}

Byte Mapper066::ReadCHR(Address address) {
  int chr_bank = (select_chr_prg_ & 0xf);
  uint32_t base_address = kCHRBankSize * chr_bank;
  return cartridge()->GetRomData()->CHR[base_address + (address % 0x2000)];
}

void Mapper066::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(select_chr_prg_);
}

bool Mapper066::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&select_chr_prg_);
  return true;
}
}  // namespace nes
}  // namespace kiwi
