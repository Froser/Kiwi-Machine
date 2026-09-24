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

#include "nes/mappers/mapper147.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;
constexpr Address kRegisterMask = 0xe103;

}  // namespace

Mapper147::Mapper147(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 147);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK(!rom_data()->PRG.empty());
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper147::~Mapper147() = default;

void Mapper147::Reset() {
  input_ = 0;
  register_ = 0;
  output_ = 0;
  increment_ = false;
  invert_ = false;
  UpdateBanks();
}

void Mapper147::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  output_ = register_ & 0x3f;
  UpdateBanks();
}

Byte Mapper147::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper147::WriteCHR(Address address, Byte value) {}

Byte Mapper147::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper147::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper147::WriteExtendedRAM(Address address, Byte value) {
  switch (address & kRegisterMask) {
    case 0x4100:
      if (increment_) {
        register_ =
            static_cast<Byte>((register_ & 0x30) | ((register_ + 1) & 0x0f));
      } else {
        register_ = static_cast<Byte>(
            (input_ & 0x30) |
            (invert_ ? static_cast<Byte>((~input_) & 0x0f) : input_ & 0x0f));
      }
      return;
    case 0x4101:
      invert_ = value & 0x04;
      return;
    case 0x4102:
      input_ = (value >> 2) & 0x3f;
      return;
    case 0x4103:
      increment_ = value & 0x04;
      return;
    default:
      Mapper::WriteExtendedRAM(address, value);
  }
}

Byte Mapper147::ReadExtendedRAM(Address address) {
  const Byte open_bus = Mapper::ReadExtendedRAM(address);
  if ((address & kRegisterMask) != 0x4100)
    return open_bus;

  Byte value = register_;
  if (invert_)
    value ^= 0x30;
  return static_cast<Byte>((value << 2) | (open_bus & 0x03));
}

void Mapper147::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(input_)
      .WriteData(register_)
      .WriteData(output_)
      .WriteData(increment_)
      .WriteData(invert_);
  Mapper::Serialize(data);
}

bool Mapper147::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&input_)
      .ReadData(&register_)
      .ReadData(&output_)
      .ReadData(&increment_)
      .ReadData(&invert_);
  UpdateBanks();
  return Mapper::Deserialize(header, data);
}

void Mapper147::UpdateBanks() {
  selected_prg_bank_ =
      static_cast<Byte>(((output_ & 0x20) >> 4) | (output_ & 0x01));
  selected_chr_bank_ = (output_ >> 1) & 0x0f;
}

}  // namespace nes
}  // namespace kiwi
