// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper133.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper133::Mapper133(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 133);
  DCHECK(!rom_data()->PRG.empty());

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRBankSize);
    chr_bank_count_ = 1;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);
  Reset();
}

Mapper133::~Mapper133() = default;

void Mapper133::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
}

void Mapper133::WritePRG(Address address, Byte value) {}

Byte Mapper133::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t index = static_cast<size_t>(selected_prg_bank_) * kPRGBankSize +
                       (address & 0x7fff);
  return rom_data()->PRG[index % rom_data()->PRG.size()];
}

void Mapper133::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper133::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper133::GetAbsoluteCHRAddress(Address address) {
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper133::WriteExtendedRAM(Address address, Byte value) {
  if ((address & 0x6100) == 0x4100) {
    SelectBanks(value);
    return;
  }
  Mapper::WriteExtendedRAM(address, value);
}

void Mapper133::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper133::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

void Mapper133::SelectBanks(Byte value) {
  selected_prg_bank_ = (value >> 2) & 0x01;
  selected_chr_bank_ = value & 0x03;
}

}  // namespace nes
}  // namespace kiwi
