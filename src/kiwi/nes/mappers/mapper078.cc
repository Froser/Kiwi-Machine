// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper078.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper078::Mapper078(Cartridge* cartridge)
    : Mapper(cartridge), holy_diver_mode_(rom_data()->submapper == 3) {
  DCHECK_EQ(rom_data()->mapper, 78);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
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

Mapper078::~Mapper078() = default;

void Mapper078::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
  mirroring_ = holy_diver_mode_ ? rom_data()->name_table_mirroring
                                : NametableMirroring::kOneScreenLower;
}

void Mapper078::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  value &= ReadPRG(address);
  selected_prg_bank_ = value & 0x07;
  selected_chr_bank_ = value >> 4;

  const NametableMirroring mirroring =
      holy_diver_mode_ ? (value & 0x08 ? NametableMirroring::kVertical
                                       : NametableMirroring::kHorizontal)
                       : (value & 0x08 ? NametableMirroring::kOneScreenHigher
                                       : NametableMirroring::kOneScreenLower);
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

Byte Mapper078::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = address < 0xc000 ? selected_prg_bank_ % prg_bank_count_
                                       : prg_bank_count_ - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper078::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper078::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper078::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

NametableMirroring Mapper078::GetNametableMirroring() {
  return mirroring_;
}

void Mapper078::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper078::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&mirroring_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
