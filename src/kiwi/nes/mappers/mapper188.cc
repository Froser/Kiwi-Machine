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

#include "nes/mappers/mapper188.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRRAMSize = 0x2000;
constexpr size_t kInternalPRGBankCount = 8;

}  // namespace

Mapper188::Mapper188(Cartridge* cartridge)
    : Mapper(cartridge), character_ram_(kCHRRAMSize) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kInternalPRGBankCount * kPRGBankSize);
  Reset();
}

Mapper188::~Mapper188() = default;

void Mapper188::Reset() {
  selected_prg_bank_ = 0;
  lower_prg_mapped_ = true;
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper188::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  if (address >= 0xc000 || lower_prg_mapped_)
    value &= ReadPRG(address);

  const bool select_internal_rom = (value & 0x10) != 0;
  const size_t bank = value & 0x07;
  if (select_internal_rom) {
    selected_prg_bank_ = static_cast<Byte>(bank);
    lower_prg_mapped_ = true;
  } else if (rom_data()->PRG.size() >=
             2 * kInternalPRGBankCount * kPRGBankSize) {
    selected_prg_bank_ = static_cast<Byte>(kInternalPRGBankCount + bank);
    lower_prg_mapped_ = true;
  } else {
    lower_prg_mapped_ = false;
  }

  SetMirroring(value & 0x20 ? NametableMirroring::kHorizontal
                            : NametableMirroring::kVertical);
}

Byte Mapper188::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  if (address < 0xc000) {
    if (!lower_prg_mapped_)
      return static_cast<Byte>(address >> 8);
    const size_t index =
        static_cast<size_t>(selected_prg_bank_) * kPRGBankSize +
        (address & 0x3fff);
    return rom_data()->PRG[index];
  }

  const size_t index =
      (kInternalPRGBankCount - 1) * kPRGBankSize + (address & 0x3fff);
  return rom_data()->PRG[index];
}

void Mapper188::WriteCHR(Address address, Byte value) {
  character_ram_[address & 0x1fff] = value;
}

Byte Mapper188::ReadCHR(Address address) {
  return character_ram_[address & 0x1fff];
}

uint32_t Mapper188::GetAbsoluteCHRAddress(Address address) {
  return address & 0x1fff;
}

void Mapper188::WriteExtendedRAM(Address address, Byte value) {}

Byte Mapper188::ReadExtendedRAM(Address address) {
  return static_cast<Byte>((address >> 8) & 0xf8) | 0x03;
}

Byte* Mapper188::GetExtendedRAMPointer() {
  return nullptr;
}

NametableMirroring Mapper188::GetNametableMirroring() {
  return mirroring_;
}

void Mapper188::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(lower_prg_mapped_)
      .WriteData(character_ram_)
      .WriteData(mirroring_);
  Mapper::Serialize(data);
}

bool Mapper188::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&lower_prg_mapped_)
      .ReadData(&character_ram_)
      .ReadData(&mirroring_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper188::SetMirroring(NametableMirroring mirroring) {
  if (mirroring_ == mirroring)
    return;

  mirroring_ = mirroring;
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
}

}  // namespace nes
}  // namespace kiwi
