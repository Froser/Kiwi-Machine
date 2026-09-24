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

#include "nes/mappers/mapper189.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr Address kBankRegisterStart = 0x4120;

}  // namespace

Mapper189::Mapper189(Cartridge* cartridge) : Mapper004(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 189);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);

  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = 0;
}

Mapper189::~Mapper189() = default;

void Mapper189::Reset() {
  selected_prg_bank_ = 0;
}

Byte Mapper189::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t index = (static_cast<size_t>(selected_prg_bank_) * kPRGBankSize +
                        (address - 0x8000)) %
                       rom_data()->PRG.size();
  return rom_data()->PRG[index];
}

void Mapper189::WriteExtendedRAM(Address address, Byte value) {
  if (address < kBankRegisterStart || address > 0x7fff)
    return;

  selected_prg_bank_ = (value | (value >> 4)) & 0x07;
}

void Mapper189::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper004::Serialize(data);
  data.WriteData(selected_prg_bank_);
}

bool Mapper189::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (!Mapper004::Deserialize(header, data))
    return false;

  data.ReadData(&selected_prg_bank_);
  return true;
}

}  // namespace nes
}  // namespace kiwi
