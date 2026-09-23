// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper046.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper046::Mapper046(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 46);
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

Mapper046::~Mapper046() = default;

void Mapper046::Reset() {
  outer_register_ = 0;
  inner_register_ = 0;
}

void Mapper046::WritePRG(Address address, Byte value) {
  if (address >= 0x8000)
    inner_register_ = value;
}

Byte Mapper046::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  return rom_data()->PRG[GetPRGBank() * kPRGBankSize + (address & 0x7fff)];
}

void Mapper046::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper046::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper046::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  return static_cast<uint32_t>(GetCHRBank() * kCHRBankSize +
                               (address & 0x1fff));
}

void Mapper046::WriteExtendedRAM(Address address, Byte value) {
  if (address >= 0x6000 && address <= 0x7fff)
    outer_register_ = value;
}

void Mapper046::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(outer_register_)
      .WriteData(inner_register_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper046::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&outer_register_)
      .ReadData(&inner_register_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

size_t Mapper046::GetPRGBank() const {
  return (((outer_register_ & 0x0f) << 1) | (inner_register_ & 0x01)) %
         prg_bank_count_;
}

size_t Mapper046::GetCHRBank() const {
  return (((outer_register_ & 0xf0) >> 1) | ((inner_register_ & 0x70) >> 4)) %
         chr_bank_count_;
}

}  // namespace nes
}  // namespace kiwi
