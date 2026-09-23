// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper070.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper070::Mapper070(Cartridge* cartridge)
    : Mapper(cartridge),
      controls_mirroring_(rom_data()->mapper == 152),
      mirroring_(rom_data()->name_table_mirroring) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GT(prg_bank_count_, 0u);

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = 1;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  Reset();
}

Mapper070::~Mapper070() = default;

void Mapper070::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
  mirroring_ = controls_mirroring_ ? NametableMirroring::kOneScreenLower
                                   : rom_data()->name_table_mirroring;
}

void Mapper070::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  // The 74*161 register and PRG-ROM both drive the CPU data bus.
  value &= ReadPRG(address);
  selected_prg_bank_ = controls_mirroring_
                           ? static_cast<Byte>((value >> 4) & 0x07)
                           : static_cast<Byte>(value >> 4);
  selected_chr_bank_ = value & 0x0f;

  if (!controls_mirroring_)
    return;

  const NametableMirroring mirroring =
      value & 0x80 ? NametableMirroring::kOneScreenHigher
                   : NametableMirroring::kOneScreenLower;
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

Byte Mapper070::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = address < 0xc000 ? selected_prg_bank_ % prg_bank_count_
                                       : prg_bank_count_ - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper070::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper070::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[GetAbsoluteCHRAddress(address)];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper070::GetAbsoluteCHRAddress(Address address) {
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

NametableMirroring Mapper070::GetNametableMirroring() {
  return mirroring_;
}

void Mapper070::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper070::Deserialize(const EmulatorStates::Header& header,
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
