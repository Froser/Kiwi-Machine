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

#include "nes/mappers/mapper077.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x0800;

}  // namespace

Mapper077::Mapper077(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 77);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  character_ram_.resize(kCHRRAMSize);
  Reset();
}

Mapper077::~Mapper077() = default;

void Mapper077::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
}

void Mapper077::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  value &= ReadPRG(address);
  selected_prg_bank_ = value & 0x0f;
  selected_chr_bank_ = (value >> 4) & 0x0f;
}

Byte Mapper077::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper077::WriteCHR(Address address, Byte value) {
  DCHECK_LT(address, 0x2000);
  if (address >= kCHRBankSize)
    character_ram_[address - kCHRBankSize] = value;
}

Byte Mapper077::ReadCHR(Address address) {
  DCHECK_LT(address, 0x2000);
  if (address >= kCHRBankSize)
    return character_ram_[address - kCHRBankSize];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper077::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, kCHRBankSize);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + address);
}

NametableMirroring Mapper077::GetNametableMirroring() {
  return NametableMirroring::kFourScreen;
}

bool Mapper077::UsesCustomPPUMemoryMapping() const {
  return true;
}

Byte Mapper077::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHR(address);
  if (address < 0x2800)
    return character_ram_[address & 0x07ff];
  return ciram[address & 0x07ff];
}

void Mapper077::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    WriteCHR(address, value);
  } else if (address < 0x2800) {
    character_ram_[address & 0x07ff] = value;
  } else {
    ciram[address & 0x07ff] = value;
  }
}

void Mapper077::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper077::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
