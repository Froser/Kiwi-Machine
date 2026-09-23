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

#include "nes/mappers/mapper115.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;

}  // namespace

Mapper115::Mapper115(Cartridge* cartridge) : Mapper004(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 115);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
}

Mapper115::~Mapper115() = default;

void Mapper115::Reset() {
  mode_register_ = 0;
  outer_chr_bank_ = 0;
}

Byte Mapper115::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = static_cast<size_t>(ResolvePRGBank(address));
  const size_t index = (bank * kPRGBankSize + (address & (kPRGBankSize - 1))) %
                       rom_data()->PRG.size();
  return rom_data()->PRG[index];
}

void Mapper115::WriteExtendedRAM(Address address, Byte value) {
  switch (address & 0xe003) {
    case 0x6000:
      mode_register_ = value;
      return;
    case 0x6001:
      outer_chr_bank_ = value & 0x01;
      return;
  }
  Mapper004::WriteExtendedRAM(address, value);
}

Byte Mapper115::ReadExtendedRAM(Address address) {
  if ((address & 0xe003) == 0x6002)
    return 0;
  return Mapper004::ReadExtendedRAM(address);
}

uint32_t Mapper115::GetAbsoluteCHRAddress(Address address) {
  if (uses_character_ram_)
    return address & 0x1fff;

  const size_t index =
      static_cast<size_t>(MapCHRBank(GetCHRBank(address))) * kCHRBankSize +
      (address & (kCHRBankSize - 1));
  return index % rom_data()->CHR.size();
}

void Mapper115::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper004::Serialize(data);
  data.WriteData(mode_register_).WriteData(outer_chr_bank_);
}

bool Mapper115::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (!Mapper004::Deserialize(header, data))
    return false;
  data.ReadData(&mode_register_).ReadData(&outer_chr_bank_);
  return true;
}

Byte Mapper115::ReadCHRByBank(int bank, Address address) {
  return Mapper004::ReadCHRByBank(MapCHRBank(bank), address);
}

int Mapper115::ResolvePRGBank(Address address) const {
  if (mode_register_ & 0x80) {
    int bank = ((mode_register_ & 0x40) >> 1) | ((mode_register_ & 0x0e) << 1);
    if (mode_register_ & 0x20) {
      bank |= (address >> 13) & 0x03;
    } else {
      bank |= ((mode_register_ & 0x01) << 1) | ((address >> 13) & 0x01);
    }
    return bank;
  }

  int bank = 0;
  switch (address & 0xe000) {
    case 0x8000:
      bank = prg_mode_ ? prg_banks_count_ - 2 : bank_register_[6];
      break;
    case 0xa000:
      bank = bank_register_[7];
      break;
    case 0xc000:
      bank = prg_mode_ ? bank_register_[6] : prg_banks_count_ - 2;
      break;
    case 0xe000:
      bank = prg_banks_count_ - 1;
      break;
  }
  return (bank & 0x1f) | ((mode_register_ & 0x40) >> 1);
}

int Mapper115::MapCHRBank(int bank) const {
  return (bank & 0xff) | (outer_chr_bank_ << 8);
}

}  // namespace nes
}  // namespace kiwi
