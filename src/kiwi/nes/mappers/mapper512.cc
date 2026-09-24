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

#include "nes/mappers/mapper512.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kCHRRAMSize = 0x2000;
constexpr size_t kCHRHalfSize = 0x1000;
constexpr size_t kPRGNVRAMSize = 0x2000;

}  // namespace

Mapper512::Mapper512(Cartridge* cartridge)
    : Mapper004(cartridge), character_ram_(kCHRRAMSize) {
  DCHECK_EQ(rom_data()->mapper, 512);
  DCHECK(!rom_data()->CHR.empty());

  rom_data()->has_battery = true;
  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = kPRGNVRAMSize;
  EnsurePRGRAM(kPRGNVRAMSize);
}

Mapper512::~Mapper512() = default;

void Mapper512::Reset() {
  mode_ = 0;
  ResetMMC3Registers();
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
}

void Mapper512::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 && (address & 0xc100) == 0x4100) {
    mode_ = value & 0x03;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
    return;
  }
  Mapper004::WriteExtendedRAM(address, value);
}

void Mapper512::WriteCHR(Address address, Byte value) {
  if (address < 0x2000 && UsesCHRRAM()) {
    character_ram_[address & (kCHRHalfSize - 1)] = value;
    return;
  }
  if (address >= 0x2000 && address < 0x3000 && UsesCartridgeVRAM()) {
    character_ram_[kCHRHalfSize + (address & (kCHRHalfSize - 1))] = value;
    return;
  }
  Mapper004::WriteCHR(address, value);
}

Byte Mapper512::ReadCHR(Address address) {
  if (address < 0x2000 && UsesCHRRAM())
    return character_ram_[address & (kCHRHalfSize - 1)];
  if (address >= 0x2000 && address < 0x3000 && UsesCartridgeVRAM()) {
    return character_ram_[kCHRHalfSize + (address & (kCHRHalfSize - 1))];
  }
  return Mapper004::ReadCHR(address);
}

uint32_t Mapper512::GetAbsoluteCHRAddress(Address address) {
  if (UsesCHRRAM())
    return address & (kCHRHalfSize - 1);
  return Mapper004::GetAbsoluteCHRAddress(address);
}

NametableMirroring Mapper512::GetNametableMirroring() {
  if (UsesCartridgeVRAM())
    return NametableMirroring::kFourScreen;
  return Mapper004::GetNametableMirroring();
}

void Mapper512::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper004::Serialize(data);
  data.WriteData(mode_).WriteData(character_ram_);
}

bool Mapper512::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (!Mapper004::Deserialize(header, data))
    return false;
  data.ReadData(&mode_).ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return true;
}

bool Mapper512::UsesCHRRAM() const {
  return (mode_ & 0x02) != 0;
}

bool Mapper512::UsesCartridgeVRAM() const {
  return mode_ == 0x01;
}

void Mapper512::ResetMMC3Registers() {
  last_vram_address_ = 0;
  target_register_ = 0;
  prg_mode_ = false;
  chr_mode_ = false;
  bank_register_[0] = 0;
  bank_register_[1] = 2;
  bank_register_[2] = 4;
  bank_register_[3] = 5;
  bank_register_[4] = 6;
  bank_register_[5] = 7;
  bank_register_[6] = 0;
  bank_register_[7] = 1;
  irq_enabled_ = false;
  irq_counter_ = 0;
  irq_latch_ = 0;
  irq_reload_ = false;
  mirroring_ = rom_data()->name_table_mirroring;
}

}  // namespace nes
}  // namespace kiwi
