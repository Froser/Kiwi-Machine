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

#include "nes/mappers/mapper047.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;

}  // namespace

Mapper047::Mapper047(Cartridge* cartridge) : Mapper004(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 47);
  DCHECK_EQ(rom_data()->PRG.size(), 32 * kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size(), 256 * kCHRBankSize);

  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = 0;
}

Mapper047::~Mapper047() = default;

void Mapper047::Reset() {
  selected_block_ = 0;
}

void Mapper047::WritePRG(Address address, Byte value) {
  if ((address & 0xe001) == 0xa001) {
    work_ram_enabled_ = value & 0x80;
    work_ram_write_protected_ = value & 0x40;
  }
  Mapper004::WritePRG(address, value);
}

Byte Mapper047::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
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

  const size_t index = static_cast<size_t>(MapPRGBank(bank)) * kPRGBankSize +
                       (address & (kPRGBankSize - 1));
  return rom_data()->PRG[index];
}

void Mapper047::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 || address > 0x7fff || !work_ram_enabled_ ||
      work_ram_write_protected_) {
    return;
  }
  selected_block_ = value & 0x01;
}

uint32_t Mapper047::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  return static_cast<uint32_t>(MapCHRBank(GetCHRBank(address))) * kCHRBankSize +
         (address & (kCHRBankSize - 1));
}

void Mapper047::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper004::Serialize(data);
  data.WriteData(selected_block_)
      .WriteData(work_ram_enabled_)
      .WriteData(work_ram_write_protected_);
}

bool Mapper047::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (!Mapper004::Deserialize(header, data))
    return false;

  data.ReadData(&selected_block_)
      .ReadData(&work_ram_enabled_)
      .ReadData(&work_ram_write_protected_);
  return true;
}

Byte Mapper047::ReadCHRByBank(int bank, Address address) {
  return Mapper004::ReadCHRByBank(MapCHRBank(bank), address);
}

int Mapper047::MapPRGBank(int bank) const {
  return (bank & 0x0f) | (selected_block_ << 4);
}

int Mapper047::MapCHRBank(int bank) const {
  return (bank & 0x7f) | (selected_block_ << 7);
}

}  // namespace nes
}  // namespace kiwi
