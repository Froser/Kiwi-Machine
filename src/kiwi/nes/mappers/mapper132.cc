// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper132.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;
constexpr Address kReadRegisterMask = 0xe100;
constexpr Address kWriteRegisterMask = 0xe103;
constexpr Address kRegisterBase = 0x4100;

}  // namespace

Mapper132::Mapper132(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 132);
  DCHECK(!rom_data()->PRG.empty());
  DCHECK(!rom_data()->CHR.empty());
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);

  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper132::~Mapper132() = default;

void Mapper132::Reset() {
  accumulator_ = 0;
  staging_ = 0;
  output_ = 0;
  s_flag_ = false;
  increment_ = false;
  invert_ = false;
  UpdateBanks();
}

void Mapper132::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  output_ = accumulator_ & 0x07;
  UpdateBanks();
}

Byte Mapper132::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t index = static_cast<size_t>(selected_prg_bank_) * kPRGBankSize +
                       (address & 0x7fff);
  return rom_data()->PRG[index % rom_data()->PRG.size()];
}

void Mapper132::WriteCHR(Address address, Byte value) {}

Byte Mapper132::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper132::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + address);
}

void Mapper132::WriteExtendedRAM(Address address, Byte value) {
  switch (address & kWriteRegisterMask) {
    case 0x4100:
      if (increment_) {
        accumulator_ = (accumulator_ + 1) & 0x07;
      } else {
        accumulator_ =
            invert_ ? static_cast<Byte>((~staging_) & 0x07) : staging_;
      }
      return;
    case 0x4101:
      invert_ = value & 0x01;
      return;
    case 0x4102:
      staging_ = value & 0x07;
      s_flag_ = value & 0x08;
      return;
    case 0x4103:
      increment_ = value & 0x01;
      return;
    default:
      Mapper::WriteExtendedRAM(address, value);
  }
}

Byte Mapper132::ReadExtendedRAM(Address address) {
  const Byte open_bus = Mapper::ReadExtendedRAM(address);
  if ((address & kReadRegisterMask) != kRegisterBase)
    return open_bus;

  const Byte protection =
      static_cast<Byte>(((s_flag_ ^ invert_) ? 0x08 : 0) | accumulator_);
  return static_cast<Byte>((open_bus & 0xf0) | protection);
}

void Mapper132::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(accumulator_)
      .WriteData(staging_)
      .WriteData(output_)
      .WriteData(s_flag_)
      .WriteData(increment_)
      .WriteData(invert_);
  Mapper::Serialize(data);
}

bool Mapper132::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&accumulator_)
      .ReadData(&staging_)
      .ReadData(&output_)
      .ReadData(&s_flag_)
      .ReadData(&increment_)
      .ReadData(&invert_);
  UpdateBanks();
  return Mapper::Deserialize(header, data);
}

void Mapper132::UpdateBanks() {
  selected_prg_bank_ = (output_ >> 2) & 0x01;
  selected_chr_bank_ = output_ & 0x03;
}

}  // namespace nes
}  // namespace kiwi
