// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper140.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper140::Mapper140(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 140);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = 1;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }

  Reset();
}

Mapper140::~Mapper140() = default;

void Mapper140::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
}

void Mapper140::WritePRG(Address address, Byte value) {}

Byte Mapper140::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper140::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper140::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper140::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper140::WriteExtendedRAM(Address address, Byte value) {
  if (address >= 0x6000 && address <= 0x7fff) {
    selected_prg_bank_ = (value >> 4) & 0x03;
    selected_chr_bank_ = value & 0x0f;
  }
}

void Mapper140::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper140::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
