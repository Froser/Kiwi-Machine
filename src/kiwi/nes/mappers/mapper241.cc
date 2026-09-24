// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper241.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;

}  // namespace

Mapper241::Mapper241(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 241);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK(!rom_data()->PRG.empty());
  DCHECK(rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  character_ram_.resize(kCHRRAMSize);
  Reset();
}

Mapper241::~Mapper241() = default;

void Mapper241::Reset() {
  selected_prg_bank_ = 0;
}

void Mapper241::WritePRG(Address address, Byte value) {
  if (address >= 0x8000)
    selected_prg_bank_ = value;
}

Byte Mapper241::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper241::WriteCHR(Address address, Byte value) {
  character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper241::ReadCHR(Address address) {
  return character_ram_[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper241::GetAbsoluteCHRAddress(Address address) {
  return address & 0x1fff;
}

void Mapper241::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_).WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper241::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_).ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
