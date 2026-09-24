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

#include "nes/mappers/mapper180.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;

}  // namespace

Mapper180::Mapper180(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 180);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
  DCHECK(rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  character_ram_.resize(kCHRRAMSize);
  Reset();
}

Mapper180::~Mapper180() = default;

void Mapper180::Reset() {
  selected_prg_bank_ = 0;
}

void Mapper180::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  selected_prg_bank_ = (value & ReadPRG(address)) & 0x07;
}

Byte Mapper180::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank =
      address < 0xc000 ? 0 : selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper180::WriteCHR(Address address, Byte value) {
  DCHECK_LT(address, 0x2000);
  character_ram_[address] = value;
}

Byte Mapper180::ReadCHR(Address address) {
  DCHECK_LT(address, 0x2000);
  return character_ram_[address];
}

void Mapper180::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_).WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper180::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_).ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
