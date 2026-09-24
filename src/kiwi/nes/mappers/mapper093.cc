// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper093.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRRAMSize = 0x2000;

}  // namespace

Mapper093::Mapper093(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 93);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
  DCHECK(rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  character_ram_.resize(kCHRRAMSize);
  Reset();
}

Mapper093::~Mapper093() = default;

void Mapper093::Reset() {
  selected_prg_bank_ = 0;
  chr_ram_enabled_ = true;
}

void Mapper093::WritePRG(Address address, Byte value) {
  DCHECK_GE(address, 0x8000);

  // The register and PRG-ROM both drive the CPU data bus.
  value &= ReadPRG(address);
  selected_prg_bank_ = static_cast<Byte>((value >> 4) & 0x07);
  chr_ram_enabled_ = (value & 0x01) != 0;
}

Byte Mapper093::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = address < 0xc000 ? selected_prg_bank_ % prg_bank_count_
                                       : prg_bank_count_ - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper093::WriteCHR(Address address, Byte value) {
  if (chr_ram_enabled_)
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper093::ReadCHR(Address address) {
  if (!chr_ram_enabled_)
    return 0xff;
  return character_ram_[address & 0x1fff];
}

uint32_t Mapper093::GetAbsoluteCHRAddress(Address address) {
  return address & 0x1fff;
}

void Mapper093::WriteExtendedRAM(Address address, Byte value) {}

Byte Mapper093::ReadExtendedRAM(Address address) {
  return static_cast<Byte>(address >> 8);
}

void Mapper093::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(chr_ram_enabled_)
      .WriteData(character_ram_);
}

bool Mapper093::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&chr_ram_enabled_)
      .ReadData(&character_ram_);
  return true;
}

}  // namespace nes
}  // namespace kiwi
