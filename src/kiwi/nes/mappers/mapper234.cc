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

#include "nes/mappers/mapper234.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

constexpr Address kOuterRegisterStart = 0xff80;
constexpr Address kOuterRegisterEnd = 0xff9f;
constexpr Address kInnerRegisterStart = 0xffe8;
constexpr Address kInnerRegisterEnd = 0xfff7;

}  // namespace

Mapper234::Mapper234(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GT(prg_bank_count_, 0u);

  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  DCHECK_GT(chr_bank_count_, 0u);

  Reset();
}

Mapper234::~Mapper234() = default;

void Mapper234::Reset() {
  outer_bank_ = 0;
  inner_bank_ = 0;
  UpdateMirroring();
}

void Mapper234::WritePRG(Address address, Byte value) {
  if ((address < kOuterRegisterStart || address > kOuterRegisterEnd) &&
      (address < kInnerRegisterStart || address > kInnerRegisterEnd)) {
    return;
  }

  // PRG-ROM remains enabled during writes, so zero bits driven by the ROM win.
  LatchRegister(address, value & ReadMappedPRG(address));
}

Byte Mapper234::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const Byte value = ReadMappedPRG(address);
  LatchRegister(address, value);
  return value;
}

void Mapper234::WriteCHR(Address address, Byte value) {}

Byte Mapper234::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper234::GetAbsoluteCHRAddress(Address address) {
  return static_cast<uint32_t>(GetCHRBank() * kCHRBankSize +
                               (address & (kCHRBankSize - 1)));
}

NametableMirroring Mapper234::GetNametableMirroring() {
  return mirroring_;
}

void Mapper234::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(outer_bank_).WriteData(inner_bank_);
  Mapper::Serialize(data);
}

bool Mapper234::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&outer_bank_).ReadData(&inner_bank_);
  UpdateMirroring();
  return Mapper::Deserialize(header, data);
}

Byte Mapper234::ReadMappedPRG(Address address) {
  const size_t index =
      GetPRGBank() * kPRGBankSize + (address & (kPRGBankSize - 1));
  return rom_data()->PRG[index];
}

void Mapper234::LatchRegister(Address address, Byte value) {
  if (address >= kOuterRegisterStart && address <= kOuterRegisterEnd) {
    if (outer_bank_ & 0x3f)
      return;
    outer_bank_ = value;
  } else if (address >= kInnerRegisterStart && address <= kInnerRegisterEnd) {
    inner_bank_ = value & 0x71;
  } else {
    return;
  }

  UpdateMirroring();
}

size_t Mapper234::GetPRGBank() const {
  const size_t bank = outer_bank_ & 0x40
                          ? (outer_bank_ & 0x0e) | (inner_bank_ & 0x01)
                          : outer_bank_ & 0x0f;
  return bank % prg_bank_count_;
}

size_t Mapper234::GetCHRBank() const {
  const size_t bank = outer_bank_ & 0x40
                          ? ((static_cast<size_t>(outer_bank_) << 2) & 0x38) |
                                ((inner_bank_ >> 4) & 0x07)
                          : ((static_cast<size_t>(outer_bank_) << 2) & 0x3c) |
                                ((inner_bank_ >> 4) & 0x03);
  return bank % chr_bank_count_;
}

void Mapper234::UpdateMirroring() {
  const NametableMirroring mirroring = outer_bank_ & 0x80
                                           ? NametableMirroring::kHorizontal
                                           : NametableMirroring::kVertical;
  if (mirroring_ == mirroring)
    return;

  mirroring_ = mirroring;
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
}

}  // namespace nes
}  // namespace kiwi
