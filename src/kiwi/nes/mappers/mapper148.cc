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

#include "nes/mappers/mapper148.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper148::Mapper148(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 148);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK(!rom_data()->PRG.empty());
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper148::~Mapper148() = default;

void Mapper148::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
}

void Mapper148::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  value &= ReadPRG(address);
  selected_prg_bank_ = (value >> 3) & 0x01;
  selected_chr_bank_ = value & 0x07;
}

Byte Mapper148::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper148::WriteCHR(Address address, Byte value) {}

Byte Mapper148::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper148::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper148::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_).WriteData(selected_chr_bank_);
  Mapper::Serialize(data);
}

bool Mapper148::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_).ReadData(&selected_chr_bank_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
