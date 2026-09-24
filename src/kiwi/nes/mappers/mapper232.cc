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

#include "nes/mappers/mapper232.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRRAMSize = 0x2000;

}  // namespace

Mapper232::Mapper232(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 4 * kPRGBankSize);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  swaps_block_bits_ = rom_data()->submapper == 1;
  character_ram_.resize(kCHRRAMSize);
}

Mapper232::~Mapper232() = default;

void Mapper232::Reset() {
  prg_block_ = 0;
  prg_page_ = 0;
}

void Mapper232::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  if (address < 0xc000) {
    prg_block_ =
        swaps_block_bits_
            ? static_cast<Byte>(((value >> 4) & 0x01) | ((value >> 2) & 0x02))
            : static_cast<Byte>((value >> 3) & 0x03);
  } else {
    prg_page_ = value & 0x03;
  }
}

Byte Mapper232::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  return rom_data()->PRG[GetPRGAddress(address)];
}

void Mapper232::WriteCHR(Address address, Byte value) {
  character_ram_[address & 0x1fff] = value;
}

Byte Mapper232::ReadCHR(Address address) {
  return character_ram_[address & 0x1fff];
}

void Mapper232::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_block_).WriteData(prg_page_).WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper232::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_block_).ReadData(&prg_page_).ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

size_t Mapper232::GetPRGAddress(Address address) const {
  const size_t bank = (static_cast<size_t>(prg_block_) << 2) |
                      (address < 0xc000 ? prg_page_ : 3);
  return (bank % prg_bank_count_) * kPRGBankSize + (address & 0x3fff);
}

}  // namespace nes
}  // namespace kiwi
