// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper079.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper079::Mapper079(Cartridge* cartridge) : Mapper079(cartridge, false) {}

Mapper079::Mapper079(Cartridge* cartridge, bool multicart_mode)
    : Mapper(cartridge),
      multicart_mode_(multicart_mode),
      mirroring_(rom_data()->name_table_mirroring) {
  DCHECK(!rom_data()->PRG.empty());

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = 1;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
}

Mapper079::~Mapper079() = default;

void Mapper079::WritePRG(Address address, Byte value) {}

Byte Mapper079::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t offset = selected_prg_bank_ * kPRGBankSize + (address & 0x7fff);
  return rom_data()->PRG[offset % rom_data()->PRG.size()];
}

void Mapper079::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper079::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

void Mapper079::WriteExtendedRAM(Address address, Byte value) {
  if ((address & 0xe100) == 0x4100) {
    SelectBanks(value);
    return;
  }
  Mapper::WriteExtendedRAM(address, value);
}

NametableMirroring Mapper079::GetNametableMirroring() {
  return mirroring_;
}

uint32_t Mapper079::GetAbsoluteCHRAddress(Address address) {
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper079::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper079::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&mirroring_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper079::SelectBanks(Byte value) {
  selected_prg_bank_ = multicart_mode_ ? static_cast<Byte>((value >> 3) & 0x07)
                                       : static_cast<Byte>((value >> 3) & 0x01);
  selected_chr_bank_ =
      multicart_mode_
          ? static_cast<Byte>((value & 0x07) | ((value >> 3) & 0x08))
          : static_cast<Byte>(value & 0x07);

  if (!multicart_mode_)
    return;

  const NametableMirroring mirroring = (value & 0x80)
                                           ? NametableMirroring::kVertical
                                           : NametableMirroring::kHorizontal;
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

}  // namespace nes
}  // namespace kiwi
