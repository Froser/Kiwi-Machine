// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper186.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kWorkRAMBankSize = 0x1000;
constexpr size_t kWorkRAMSize = 0x10000;
constexpr size_t kCHRRAMSize = 0x2000;
constexpr size_t kNametableRAMSize = 0x1000;
constexpr uint16_t kReadyDelayCycles = 100;

}  // namespace

Mapper186::Mapper186(Cartridge* cartridge)
    : Mapper(cartridge),
      work_ram_(kWorkRAMSize),
      character_ram_(kCHRRAMSize),
      nametable_ram_(kNametableRAMSize) {
  DCHECK_EQ(rom_data()->mapper, 186);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  DCHECK(rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  rom_data()->has_battery = false;
  rom_data()->prg_ram_size = kWorkRAMSize;
  rom_data()->prg_nvram_size = 0;
  rom_data()->name_table_mirroring = NametableMirroring::kFourScreen;
  Reset();
}

Mapper186::~Mapper186() = default;

void Mapper186::Reset() {
  selected_prg_bank_ = 0;
  ram_control_ = 0;
  tape_control_ = 0;
  command_ = 0;
  command_bit_count_ = 0;
  ready_delay_ = 0;
  ready_for_bit_ = false;
}

void Mapper186::WritePRG(Address address, Byte value) {}

Byte Mapper186::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank =
      address < 0xc000 ? selected_prg_bank_ % prg_bank_count_ : 0;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper186::WriteCHR(Address address, Byte value) {
  if (address < 0x2000) {
    character_ram_[address] = value;
  } else if (address < 0x3000) {
    nametable_ram_[address - 0x2000] = value;
  }
}

Byte Mapper186::ReadCHR(Address address) {
  if (address < 0x2000)
    return character_ram_[address];
  if (address < 0x3000)
    return nametable_ram_[address - 0x2000];
  return 0;
}

uint32_t Mapper186::GetAbsoluteCHRAddress(Address address) {
  return address & 0x1fff;
}

void Mapper186::WriteExtendedRAM(Address address, Byte value) {
  if (address >= 0x4200 && address <= 0x4203) {
    WriteRegister(address, value);
    return;
  }

  if ((address >= 0x4400 && address < 0x8000))
    work_ram_[GetWorkRAMAddress(address)] = value;
}

Byte Mapper186::ReadExtendedRAM(Address address) {
  if (address >= 0x4200 && address <= 0x4203)
    return ReadRegister(address);
  if (address >= 0x4400 && address < 0x8000)
    return work_ram_[GetWorkRAMAddress(address)];
  return static_cast<Byte>(address >> 8);
}

Byte* Mapper186::GetExtendedRAMPointer() {
  return nullptr;
}

bool Mapper186::UsesCustomPRGRAM() const {
  return true;
}

NametableMirroring Mapper186::GetNametableMirroring() {
  return NametableMirroring::kFourScreen;
}

bool Mapper186::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper186::M2CycleIRQ() {
  if (ready_delay_ == 0)
    return;
  if (--ready_delay_ == 0)
    ready_for_bit_ = true;
}

void Mapper186::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(ram_control_)
      .WriteData(tape_control_)
      .WriteData(command_)
      .WriteData(command_bit_count_)
      .WriteData(ready_delay_)
      .WriteData(ready_for_bit_)
      .WriteData(work_ram_)
      .WriteData(character_ram_)
      .WriteData(nametable_ram_);
  Mapper::Serialize(data);
}

bool Mapper186::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&ram_control_)
      .ReadData(&tape_control_)
      .ReadData(&command_)
      .ReadData(&command_bit_count_)
      .ReadData(&ready_delay_)
      .ReadData(&ready_for_bit_)
      .ReadData(&work_ram_)
      .ReadData(&character_ram_)
      .ReadData(&nametable_ram_);
  return Mapper::Deserialize(header, data);
}

size_t Mapper186::GetWorkRAMAddress(Address address) const {
  size_t bank = 0;
  if (address < 0x5000) {
    bank = 8;
  } else if (address < 0x6000) {
    bank = (ram_control_ & 0x07) + 8;
  } else {
    bank = ((ram_control_ & 0xc0) >> 5) + ((address >> 12) & 0x01);
  }
  return bank * kWorkRAMBankSize + (address & 0x0fff);
}

Byte Mapper186::ReadRegister(Address address) {
  switch (address) {
    case 0x4200:
      return 0xaa;
    case 0x4201:
      return (tape_control_ & 0x01) != 0 ? 0x80 : 0;
    case 0x4202:
      return ready_for_bit_ ? 0x40 : 0;
    default:
      return 0;
  }
}

void Mapper186::WriteRegister(Address address, Byte value) {
  switch (address) {
    case 0x4200:
      ram_control_ = value;
      break;
    case 0x4201:
      selected_prg_bank_ = value;
      break;
    case 0x4202:
      if ((tape_control_ & 0x10) != 0 && (value & 0x10) == 0) {
        command_ = static_cast<Byte>((command_ << 1) | (value >> 7));
        command_bit_count_ = static_cast<Byte>((command_bit_count_ + 1) & 0x07);
      }
      if ((value & 0x10) != 0) {
        ready_for_bit_ = false;
        ready_delay_ = kReadyDelayCycles;
      }
      if ((tape_control_ & 0x20) != 0 && (value & 0x20) == 0) {
        command_ = 0;
        command_bit_count_ = 0;
        ready_for_bit_ = true;
      }
      tape_control_ = value;
      break;
    default:
      break;
  }
}

}  // namespace nes
}  // namespace kiwi
