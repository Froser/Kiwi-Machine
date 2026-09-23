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

#include "nes/mappers/mapper246.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0800;

}  // namespace

Mapper246::Mapper246(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 246);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 4 * kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper246::~Mapper246() = default;

void Mapper246::Reset() {
  prg_banks_.fill(0);
  prg_banks_[3] = 0xff;
  chr_banks_.fill(0);
}

void Mapper246::WritePRG(Address address, Byte value) {}

Byte Mapper246::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t slot = (address - 0x8000) / kPRGBankSize;
  size_t bank = prg_banks_[slot];
  if ((address & 0xffe4) == 0xffe4)
    bank |= 0x10;
  bank %= prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x1fff)];
}

void Mapper246::WriteCHR(Address address, Byte value) {}

Byte Mapper246::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper246::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t slot = address / kCHRBankSize;
  const size_t bank = chr_banks_[slot] % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x07ff));
}

void Mapper246::WriteExtendedRAM(Address address, Byte value) {
  if (address >= 0x6000 && address <= 0x6003) {
    prg_banks_[address & 0x03] = value;
  } else if (address >= 0x6004 && address <= 0x6007) {
    chr_banks_[address & 0x03] = value;
  } else if (address >= 0x6800 && address <= 0x6fff) {
    Mapper::WriteExtendedRAM(static_cast<Address>(0x6000 + (address & 0x07ff)),
                             value);
  }
}

Byte Mapper246::ReadExtendedRAM(Address address) {
  if (address >= 0x6800 && address <= 0x6fff) {
    return Mapper::ReadExtendedRAM(
        static_cast<Address>(0x6000 + (address & 0x07ff)));
  }
  return static_cast<Byte>(address >> 8);
}

void Mapper246::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_banks_).WriteData(chr_banks_);
  Mapper::Serialize(data);
}

bool Mapper246::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_banks_).ReadData(&chr_banks_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
