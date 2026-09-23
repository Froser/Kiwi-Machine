// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper184.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGSize = 0x8000;
constexpr size_t kCHRBankSize = 0x1000;

}  // namespace

Mapper184::Mapper184(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 184);
  DCHECK_EQ(rom_data()->PRG.size(), kPRGSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper184::~Mapper184() = default;

void Mapper184::Reset() {
  chr_banks_[0] = 0;
  chr_banks_[1] = 4;
}

void Mapper184::WritePRG(Address address, Byte value) {}

Byte Mapper184::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  return rom_data()->PRG[address - 0x8000];
}

void Mapper184::WriteCHR(Address address, Byte value) {}

Byte Mapper184::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper184::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t window = (address >> 12) & 0x01;
  const size_t bank = chr_banks_[window] % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x0fff));
}

void Mapper184::WriteExtendedRAM(Address address, Byte value) {
  if (address >= 0x6000 && address <= 0x7fff) {
    chr_banks_[0] = value & 0x07;
    chr_banks_[1] = static_cast<Byte>(4 | ((value >> 4) & 0x03));
    return;
  }
  Mapper::WriteExtendedRAM(address, value);
}

Byte Mapper184::ReadExtendedRAM(Address address) {
  if (address >= 0x6000 && address <= 0x7fff)
    return static_cast<Byte>(address >> 8);
  return Mapper::ReadExtendedRAM(address);
}

void Mapper184::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(chr_banks_);
}

bool Mapper184::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&chr_banks_);
  return true;
}

}  // namespace nes
}  // namespace kiwi
