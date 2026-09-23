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

#include "nes/mappers/mapper086.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper086::Mapper086(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 86);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = 0;
  Reset();
}

Mapper086::~Mapper086() = default;

void Mapper086::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
}

void Mapper086::WritePRG(Address address, Byte value) {
  if ((address & 0xf000) == 0xe000)
    SelectBanks(value);
}

Byte Mapper086::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper086::WriteCHR(Address address, Byte value) {}

Byte Mapper086::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper086::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper086::WriteExtendedRAM(Address address, Byte value) {
  if ((address & 0xf000) == 0x6000)
    SelectBanks(value);
}

void Mapper086::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_).WriteData(selected_chr_bank_);
  Mapper::Serialize(data);
}

bool Mapper086::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_).ReadData(&selected_chr_bank_);
  return Mapper::Deserialize(header, data);
}

void Mapper086::SelectBanks(Byte value) {
  selected_prg_bank_ = (value >> 4) & 0x03;
  selected_chr_bank_ = (value & 0x03) | ((value >> 4) & 0x04);
}

}  // namespace nes
}  // namespace kiwi
