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

#include "nes/mappers/mapper119.h"

#include "base/check.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kCHRRAMSize = 0x2000;

}  // namespace

Mapper119::Mapper119(Cartridge* cartridge) : Mapper004(cartridge) {
  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = 0;
  tqrom_character_ram_.resize(kCHRRAMSize);
}

Mapper119::~Mapper119() = default;

void Mapper119::WriteCHR(Address address, Byte value) {
  if (address > 0x1fff) {
    Mapper004::WriteCHR(address, value);
    return;
  }

  const int bank = GetCHRBank(address);
  if (bank & 0x40) {
    const size_t index = (static_cast<size_t>(bank) & 0x07) * kCHRBankSize +
                         (address & (kCHRBankSize - 1));
    tqrom_character_ram_[index] = value;
  }
}

void Mapper119::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper004::Serialize(data);
  data.WriteData(tqrom_character_ram_);
}

bool Mapper119::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (!Mapper004::Deserialize(header, data))
    return false;

  data.ReadData(&tqrom_character_ram_);
  return true;
}

Byte Mapper119::ReadCHRByBank(int bank, Address address) {
  if (bank & 0x40) {
    const size_t index = (static_cast<size_t>(bank) & 0x07) * kCHRBankSize +
                         (address & (kCHRBankSize - 1));
    return tqrom_character_ram_[index];
  }

  return Mapper004::ReadCHRByBank(bank & 0x3f, address);
}

}  // namespace nes
}  // namespace kiwi
