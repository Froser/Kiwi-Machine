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

#include "nes/mappers/mapper013.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGROMSize = 0x8000;
constexpr size_t kCHRBankSize = 0x1000;

}  // namespace

Mapper013::Mapper013(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 13);
  DCHECK_EQ(rom_data()->PRG.size(), kPRGROMSize);
  DCHECK(rom_data()->CHR.empty());
  character_ram_.resize(kCHRRAMSize);
  Reset();
}

Mapper013::~Mapper013() = default;

void Mapper013::Reset() {
  selected_chr_bank_ = 0;
}

void Mapper013::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  selected_chr_bank_ = (value & ReadPRG(address)) & 0x03;
}

Byte Mapper013::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  return rom_data()->PRG[address - 0x8000];
}

void Mapper013::WriteCHR(Address address, Byte value) {
  character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper013::ReadCHR(Address address) {
  return character_ram_[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper013::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = address < kCHRBankSize ? 0 : selected_chr_bank_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x0fff));
}

NametableMirroring Mapper013::GetNametableMirroring() {
  return NametableMirroring::kVertical;
}

void Mapper013::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_chr_bank_).WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper013::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_chr_bank_).ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
