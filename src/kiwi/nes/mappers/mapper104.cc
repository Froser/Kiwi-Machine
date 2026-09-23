// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper104.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kPRGBanksPerBlock = 16;

}  // namespace

Mapper104::Mapper104(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GT(prg_bank_count_, 0u);
  character_ram_.resize(kCHRRAMSize);
}

Mapper104::~Mapper104() = default;

void Mapper104::Reset() {
  if (!initialized_) {
    prg_bank_ = 0;
    initialized_ = true;
  }
}

void Mapper104::WritePRG(Address address, Byte value) {
  if (address >= 0xc000) {
    prg_bank_ = static_cast<Byte>((prg_bank_ & 0x70) | (value & 0x0f));
  } else if (address <= 0x9fff && (value & 0x08)) {
    prg_bank_ = static_cast<Byte>((prg_bank_ & 0x0f) | ((value << 4) & 0x70));
  }
}

Byte Mapper104::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  size_t bank = prg_bank_;
  if (address >= 0xc000)
    bank = (prg_bank_ & 0x70) | (kPRGBanksPerBlock - 1);
  bank %= prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper104::WriteCHR(Address address, Byte value) {
  character_ram_[address & 0x1fff] = value;
}

Byte Mapper104::ReadCHR(Address address) {
  return character_ram_[address & 0x1fff];
}

void Mapper104::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_bank_).WriteData(initialized_).WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper104::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_bank_).ReadData(&initialized_).ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
