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

#include "nes/mappers/mapper216.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper216::Mapper216(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 216);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK(!rom_data()->PRG.empty());
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRBankSize);
    chr_bank_count_ = 1;
  } else {
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  Reset();
}

Mapper216::~Mapper216() = default;

void Mapper216::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
}

void Mapper216::WritePRG(Address address, Byte value) {
  if (address >= 0x8000)
    SelectBanks(address);
}

Byte Mapper216::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper216::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper216::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper216::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper216::WriteExtendedRAM(Address address, Byte value) {
  if (address == 0x5000) {
    SelectBanks(address);
    return;
  }
  Mapper::WriteExtendedRAM(address, value);
}

Byte Mapper216::ReadExtendedRAM(Address address) {
  if (address == 0x5000)
    return 0;
  return Mapper::ReadExtendedRAM(address);
}

void Mapper216::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper216::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

void Mapper216::SelectBanks(Address address) {
  selected_prg_bank_ = address & 0x01;
  selected_chr_bank_ = (address >> 1) & 0x07;
}

}  // namespace nes
}  // namespace kiwi
