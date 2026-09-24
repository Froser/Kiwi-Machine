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

#include "nes/mappers/mapper245.h"

#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr int kBanksPerBlock = 0x40;

}  // namespace

Mapper245::Mapper245(Cartridge* cartridge) : Mapper004(cartridge) {}

Mapper245::~Mapper245() = default;

Byte Mapper245::ReadPRG(Address address) {
  const int outer_bank =
      prg_banks_count_ >= kBanksPerBlock && (bank_register_[0] & 0x02)
          ? kBanksPerBlock
          : 0;
  const int last_bank = prg_banks_count_ >= kBanksPerBlock
                            ? outer_bank + kBanksPerBlock - 1
                            : static_cast<int>(prg_banks_count_) - 1;

  int bank = 0;
  if (address < 0xa000) {
    bank = prg_mode_ ? last_bank - 1 : outer_bank + (bank_register_[6] & 0x3f);
  } else if (address < 0xc000) {
    bank = outer_bank + (bank_register_[7] & 0x3f);
  } else if (address < 0xe000) {
    bank = prg_mode_ ? outer_bank + (bank_register_[6] & 0x3f) : last_bank - 1;
  } else {
    bank = last_bank;
  }

  const size_t index =
      (static_cast<size_t>(bank) * kPRGBankSize | (address & 0x1fff)) %
      rom_data()->PRG.size();
  return rom_data()->PRG[index];
}

}  // namespace nes
}  // namespace kiwi
