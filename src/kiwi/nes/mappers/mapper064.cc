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

#include "nes/mappers/mapper064.h"

#include <algorithm>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kCHRRAMSize = 0x2000;

}  // namespace

Mapper064::Mapper064(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 4 * kPRGBankSize);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;

  uses_character_ram_ = rom_data()->CHR.empty();
  if (uses_character_ram_) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = character_ram_.size() / kCHRBankSize;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  ResetRegisters();
}

Mapper064::~Mapper064() = default;

void Mapper064::Reset() {
  ResetRegisters();
}

void Mapper064::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  switch (address & 0xe001) {
    case 0x8000:
      selected_register_ = value & 0x0f;
      one_k_chr_mode_ = (value & 0x20) != 0;
      prg_mode_ = (value & 0x40) != 0;
      chr_mode_ = (value & 0x80) != 0;
      break;
    case 0x8001:
      bank_registers_[selected_register_] = value;
      break;
    case 0xa000:
      mirroring_ = value & 0x01 ? NametableMirroring::kHorizontal
                                : NametableMirroring::kVertical;
      if (mirroring_changed_callback())
        mirroring_changed_callback().Run();
      break;
    case 0xc000:
      irq_reload_value_ = value;
      break;
    case 0xc001:
      force_cpu_clock_ = irq_cycle_mode_ && !(value & 0x01);
      irq_cycle_mode_ = (value & 0x01) != 0;
      if (irq_cycle_mode_)
        cpu_clock_counter_ = 0;
      irq_reload_pending_ = true;
      break;
    case 0xe000:
      irq_enabled_ = false;
      break;
    case 0xe001:
      irq_enabled_ = true;
      break;
  }
}

Byte Mapper064::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t window = (address - 0x8000) / kPRGBankSize;

  Byte bank = 0;
  switch (window) {
    case 0:
      bank = prg_mode_ ? bank_registers_[15] : bank_registers_[6];
      break;
    case 1:
      bank = bank_registers_[7];
      break;
    case 2:
      bank = prg_mode_ ? bank_registers_[6] : bank_registers_[15];
      break;
    case 3:
      bank = static_cast<Byte>(prg_bank_count_ - 1);
      break;
  }
  return ReadPRGBank(bank, address);
}

void Mapper064::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[GetCHRAddress(address)] = value;
}

Byte Mapper064::ReadCHR(Address address) {
  const size_t index = GetCHRAddress(address);
  return uses_character_ram_ ? character_ram_[index] : rom_data()->CHR[index];
}

uint32_t Mapper064::GetAbsoluteCHRAddress(Address address) {
  if (uses_character_ram_)
    return address & 0x1fff;
  return static_cast<uint32_t>(GetCHRAddress(address));
}

NametableMirroring Mapper064::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper064::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper064::M2CycleIRQ() {
  if (irq_delay_ > 0) {
    --irq_delay_;
    if (irq_delay_ == 0 && irq_callback())
      irq_callback().Run();
  }

  if (!irq_cycle_mode_ && !force_cpu_clock_)
    return;

  cpu_clock_counter_ = (cpu_clock_counter_ + 1) & 0x03;
  if (cpu_clock_counter_ == 0) {
    ClockIRQCounter(1);
    force_cpu_clock_ = false;
  }
}

void Mapper064::ScanlineIRQ(int scanline, bool render_enabled) {
  if (!irq_cycle_mode_ && render_enabled && scanline < 240)
    ClockIRQCounter(2);
}

void Mapper064::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(bank_registers_)
      .WriteData(selected_register_)
      .WriteData(one_k_chr_mode_)
      .WriteData(prg_mode_)
      .WriteData(chr_mode_)
      .WriteData(irq_enabled_)
      .WriteData(irq_cycle_mode_)
      .WriteData(irq_reload_pending_)
      .WriteData(irq_counter_)
      .WriteData(irq_reload_value_)
      .WriteData(cpu_clock_counter_)
      .WriteData(irq_delay_)
      .WriteData(force_cpu_clock_)
      .WriteData(mirroring_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper064::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&bank_registers_)
      .ReadData(&selected_register_)
      .ReadData(&one_k_chr_mode_)
      .ReadData(&prg_mode_)
      .ReadData(&chr_mode_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_cycle_mode_)
      .ReadData(&irq_reload_pending_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_reload_value_)
      .ReadData(&cpu_clock_counter_)
      .ReadData(&irq_delay_)
      .ReadData(&force_cpu_clock_)
      .ReadData(&mirroring_);
  if (uses_character_ram_)
    data.ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper064::ResetRegisters() {
  bank_registers_.fill(0);
  bank_registers_[0] = 0;
  bank_registers_[1] = 2;
  bank_registers_[2] = 4;
  bank_registers_[3] = 5;
  bank_registers_[4] = 6;
  bank_registers_[5] = 7;
  bank_registers_[6] = 0;
  bank_registers_[7] = 1;
  bank_registers_[8] = 8;
  bank_registers_[9] = 9;
  bank_registers_[15] = 2;

  selected_register_ = 0;
  one_k_chr_mode_ = false;
  prg_mode_ = false;
  chr_mode_ = false;
  irq_enabled_ = false;
  irq_cycle_mode_ = false;
  irq_reload_pending_ = false;
  irq_counter_ = 0;
  irq_reload_value_ = 0;
  cpu_clock_counter_ = 0;
  irq_delay_ = 0;
  force_cpu_clock_ = false;
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper064::ClockIRQCounter(Byte delay) {
  if (irq_reload_pending_) {
    irq_counter_ =
        static_cast<Byte>(irq_reload_value_ + (irq_reload_value_ <= 1 ? 1 : 2));
    irq_reload_pending_ = false;
  } else if (irq_counter_ == 0) {
    irq_counter_ = static_cast<Byte>(irq_reload_value_ + 1);
  }

  --irq_counter_;
  if (irq_counter_ == 0 && irq_enabled_)
    irq_delay_ = delay;
}

size_t Mapper064::GetCHRAddress(Address address) const {
  const size_t window = ((address >> 10) & 0x07) ^ (chr_mode_ ? 4 : 0);
  Byte bank = 0;
  switch (window) {
    case 0:
      bank = bank_registers_[0] & 0xfe;
      break;
    case 1:
      bank = one_k_chr_mode_ ? bank_registers_[8] : bank_registers_[0] | 0x01;
      break;
    case 2:
      bank = bank_registers_[1] & 0xfe;
      break;
    case 3:
      bank = one_k_chr_mode_ ? bank_registers_[9] : bank_registers_[1] | 0x01;
      break;
    default:
      bank = bank_registers_[window - 2];
      break;
  }
  return (static_cast<size_t>(bank) % chr_bank_count_) * kCHRBankSize +
         (address & 0x03ff);
}

Byte Mapper064::ReadPRGBank(Byte bank, Address address) {
  const size_t index =
      (static_cast<size_t>(bank) % prg_bank_count_) * kPRGBankSize +
      (address & 0x1fff);
  return rom_data()->PRG[index];
}

}  // namespace nes
}  // namespace kiwi
