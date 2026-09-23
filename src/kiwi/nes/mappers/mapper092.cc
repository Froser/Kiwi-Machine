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

#include "nes/mappers/mapper092.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper092::Mapper092(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 92);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = 1;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = 0;
  Reset();
}

Mapper092::~Mapper092() = default;

void Mapper092::Reset() {
  selected_prg_bank_ = static_cast<Byte>(prg_bank_count_ - 1);
  selected_chr_bank_ = 0;
  prg_latch_high_ = false;
  chr_latch_high_ = false;
}

void Mapper092::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  value &= ReadPRG(address);

  const bool prg_latch_high = (value & 0x80) != 0;
  const bool chr_latch_high = (value & 0x40) != 0;
  if (!prg_latch_high_ && prg_latch_high)
    selected_prg_bank_ = value & 0x0f;
  if (!chr_latch_high_ && chr_latch_high)
    selected_chr_bank_ = value & 0x0f;

  prg_latch_high_ = prg_latch_high;
  chr_latch_high_ = chr_latch_high;
}

Byte Mapper092::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank =
      address < 0xc000 ? 0 : selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper092::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper092::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[GetAbsoluteCHRAddress(address)];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper092::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper092::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(prg_latch_high_)
      .WriteData(chr_latch_high_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper092::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&prg_latch_high_)
      .ReadData(&chr_latch_high_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
