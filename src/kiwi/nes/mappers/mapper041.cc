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

#include "nes/mappers/mapper041.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper041::Mapper041(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 41);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper041::~Mapper041() = default;

void Mapper041::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
  SetMirroring(NametableMirroring::kVertical);
}

void Mapper041::WritePRG(Address address, Byte value) {
  if (address < 0x8000 || selected_prg_bank_ < 4)
    return;

  value &= ReadPRG(address);
  selected_chr_bank_ = (selected_chr_bank_ & 0x0c) | (value & 0x03);
}

Byte Mapper041::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper041::WriteCHR(Address address, Byte value) {}

Byte Mapper041::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper041::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper041::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 || address > 0x67ff)
    return;

  selected_prg_bank_ = address & 0x07;
  selected_chr_bank_ = (selected_chr_bank_ & 0x03) | ((address >> 1) & 0x0c);
  SetMirroring(address & 0x20 ? NametableMirroring::kHorizontal
                              : NametableMirroring::kVertical);
}

NametableMirroring Mapper041::GetNametableMirroring() {
  return mirroring_;
}

void Mapper041::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(mirroring_);
  Mapper::Serialize(data);
}

bool Mapper041::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&mirroring_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper041::SetMirroring(NametableMirroring mirroring) {
  if (mirroring_ == mirroring)
    return;

  mirroring_ = mirroring;
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
}

}  // namespace nes
}  // namespace kiwi
