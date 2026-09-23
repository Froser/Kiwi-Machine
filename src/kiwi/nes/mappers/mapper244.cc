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

#include "nes/mappers/mapper244.h"

#include <array>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

constexpr std::array<std::array<Byte, 4>, 4> kPRGBanks = {{
    {0, 1, 2, 3},
    {3, 2, 1, 0},
    {0, 2, 1, 3},
    {3, 1, 2, 0},
}};

constexpr std::array<std::array<Byte, 8>, 8> kCHRBanks = {{
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 2, 1, 3, 4, 6, 5, 7},
    {0, 1, 4, 5, 2, 3, 6, 7},
    {0, 4, 1, 5, 2, 6, 3, 7},
    {0, 4, 2, 6, 1, 5, 3, 7},
    {0, 2, 4, 6, 1, 3, 5, 7},
    {7, 6, 5, 4, 3, 2, 1, 0},
    {7, 6, 5, 4, 3, 2, 1, 0},
}};

}  // namespace

Mapper244::Mapper244(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 244);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper244::~Mapper244() = default;

void Mapper244::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
}

void Mapper244::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  if (value & 0x08) {
    selected_chr_bank_ = kCHRBanks[(value >> 4) & 0x07][value & 0x07];
  } else {
    selected_prg_bank_ = kPRGBanks[(value >> 4) & 0x03][value & 0x03];
  }
}

Byte Mapper244::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper244::WriteCHR(Address address, Byte value) {}

Byte Mapper244::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper244::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper244::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_).WriteData(selected_chr_bank_);
  Mapper::Serialize(data);
}

bool Mapper244::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_).ReadData(&selected_chr_bank_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
