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

#include "nes/mappers/mapper096.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x1000;

}  // namespace

Mapper096::Mapper096(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 96);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  DCHECK(rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  character_ram_.resize(kCHRRAMSize);
  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = 0;
  Reset();
}

Mapper096::~Mapper096() = default;

void Mapper096::Reset() {
  selected_prg_bank_ = 0;
  outer_chr_bank_ = 0;
  inner_chr_bank_ = 0;
  last_ppu_address_ = 0;
}

void Mapper096::WritePRG(Address address, Byte value) {
  DCHECK_GE(address, 0x8000);
  SelectOuterBanks(value & ReadPRG(address));
}

Byte Mapper096::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper096::WriteCHR(Address address, Byte value) {
  DCHECK_LT(address, 0x2000);
  character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper096::ReadCHR(Address address) {
  DCHECK_LT(address, 0x2000);
  return character_ram_[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper096::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank =
      outer_chr_bank_ | (address < 0x1000 ? inner_chr_bank_ : 0x03);
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x0fff));
}

NametableMirroring Mapper096::GetNametableMirroring() {
  return NametableMirroring::kVertical;
}

void Mapper096::PPUAddressChanged(Address address) {
  const bool was_nametable = (last_ppu_address_ & 0x3000) == 0x2000;
  const bool is_nametable = (address & 0x3000) == 0x2000;
  if (!was_nametable && is_nametable)
    inner_chr_bank_ = (address >> 8) & 0x03;
  last_ppu_address_ = address;
}

bool Mapper096::NeedsPPUAddressNotifications() const {
  return true;
}

void Mapper096::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(outer_chr_bank_)
      .WriteData(inner_chr_bank_)
      .WriteData(last_ppu_address_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper096::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&outer_chr_bank_)
      .ReadData(&inner_chr_bank_)
      .ReadData(&last_ppu_address_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

void Mapper096::SelectOuterBanks(Byte value) {
  selected_prg_bank_ = value & 0x03;
  outer_chr_bank_ = value & 0x04;
}

}  // namespace nes
}  // namespace kiwi
