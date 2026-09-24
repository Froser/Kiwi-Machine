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

#include "nes/mappers/mapper037.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;

}  // namespace

Mapper037::Mapper037(Cartridge* cartridge) : Mapper004(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 37);
  DCHECK_EQ(rom_data()->PRG.size(), 32 * kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size(), 256 * kCHRBankSize);

  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = 0;
}

Mapper037::~Mapper037() = default;

void Mapper037::Reset() {
  selected_block_ = 0;
}

void Mapper037::WritePRG(Address address, Byte value) {
  if ((address & 0xe001) == 0xa001) {
    work_ram_enabled_ = value & 0x80;
    work_ram_write_protected_ = value & 0x40;
  }
  Mapper004::WritePRG(address, value);
}

Byte Mapper037::ReadPRG(Address address) {
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

void Mapper037::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 || address > 0x7fff || !work_ram_enabled_ ||
      work_ram_write_protected_) {
    return;
  }
  selected_block_ = value & 0x07;
}

uint32_t Mapper037::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  return static_cast<uint32_t>(MapCHRBank(GetCHRBank(address))) * kCHRBankSize +
         (address & (kCHRBankSize - 1));
}

void Mapper037::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper004::Serialize(data);
  data.WriteData(selected_block_)
      .WriteData(work_ram_enabled_)
      .WriteData(work_ram_write_protected_);
}

bool Mapper037::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (!Mapper004::Deserialize(header, data))
    return false;

  data.ReadData(&selected_block_)
      .ReadData(&work_ram_enabled_)
      .ReadData(&work_ram_write_protected_);
  return true;
}

Byte Mapper037::ReadCHRByBank(int bank, Address address) {
  return Mapper004::ReadCHRByBank(MapCHRBank(bank), address);
}

int Mapper037::MapPRGBank(int bank) const {
  switch (selected_block_) {
    case 3:
      return (bank & 0x07) | 0x08;
    case 4:
    case 5:
    case 6:
      return (bank & 0x0f) | 0x10;
    case 7:
      return (bank & 0x07) | 0x18;
    default:
      return bank & 0x07;
  }
}

int Mapper037::MapCHRBank(int bank) const {
  return (bank & 0x7f) | ((selected_block_ & 0x04) << 5);
}

}  // namespace nes
}  // namespace kiwi
