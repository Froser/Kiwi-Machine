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

#include "nes/mappers/mapper011.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

constexpr size_t kPRGBankSize = 32 * 1024;
constexpr size_t kCHRBankSize = 8 * 1024;

Mapper011::Mapper011(Cartridge* cartridge) : Mapper(cartridge) {}

Mapper011::~Mapper011() = default;

void Mapper011::WritePRG(Address address, Byte value) {
  if (address >= 0x8000) {
    // 7  bit  0
    // ---- ----
    // CCCC LLPP
    // |||| ||||
    // |||| ||++- Select 32 KB PRG ROM bank for CPU $8000-$FFFF
    // |||| ++--- Used for lockout defeat
    // ++++------ Select 8 KB CHR ROM bank for PPU $0000-$1FFF
    prg_ = value & 0x3;
    chr_ = (value >> 4) & 0xf;
  }
}

Byte Mapper011::ReadPRG(Address address) {
  DCHECK(address >= 0x8000);
  return rom_data()->PRG[kPRGBankSize * prg_ + (address - 0x8000)];
}

void Mapper011::WriteCHR(Address address, Byte value) {}

Byte Mapper011::ReadCHR(Address address) {
  DCHECK(address < 0x2000);
  return rom_data()->CHR[kCHRBankSize * chr_ + address];
}

void Mapper011::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_).WriteData(chr_);
  Mapper::Serialize(data);
}

bool Mapper011::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_).ReadData(&chr_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
