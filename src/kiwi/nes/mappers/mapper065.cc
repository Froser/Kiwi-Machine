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

#include "nes/mappers/mapper065.h"

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

Mapper065::Mapper065(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GE(prg_bank_count_, 4u);

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

Mapper065::~Mapper065() = default;

void Mapper065::Reset() {
  ResetRegisters();
}

void Mapper065::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  const Address reg = address & 0xf007;
  if (reg >= 0x8000 && reg <= 0x8007) {
    prg_banks_[0] = value;
    return;
  }
  if (reg >= 0xa000 && reg <= 0xa007) {
    prg_banks_[1] = value;
    return;
  }
  if (reg >= 0xb000 && reg <= 0xb007) {
    chr_banks_[reg & 0x07] = value;
    return;
  }

  switch (reg) {
    case 0x9000:
      prg_mode_ = (value & 0x80) != 0;
      break;
    case 0x9001:
      switch (value & 0xc0) {
        case 0x00:
          mirroring_ = NametableMirroring::kVertical;
          break;
        case 0x80:
          mirroring_ = NametableMirroring::kHorizontal;
          break;
        default:
          mirroring_ = NametableMirroring::kOneScreenLower;
          break;
      }
      if (mirroring_changed_callback())
        mirroring_changed_callback().Run();
      break;
    case 0x9003:
      irq_enabled_ = (value & 0x80) != 0;
      break;
    case 0x9004:
      irq_counter_ = irq_reload_value_;
      break;
    case 0x9005:
      irq_reload_value_ =
          static_cast<uint16_t>((irq_reload_value_ & 0x00ff) | (value << 8));
      break;
    case 0x9006:
      irq_reload_value_ =
          static_cast<uint16_t>((irq_reload_value_ & 0xff00) | value);
      break;
  }
}

Byte Mapper065::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t window = (address - 0x8000) / kPRGBankSize;
  const size_t fixed_bank = prg_bank_count_ - 2;

  size_t bank = prg_bank_count_ - 1;
  switch (window) {
    case 0:
      bank = prg_mode_ ? fixed_bank : prg_banks_[0];
      break;
    case 1:
      bank = prg_banks_[1];
      break;
    case 2:
      bank = prg_mode_ ? prg_banks_[0] : fixed_bank;
      break;
    case 3:
      break;
  }
  return ReadPRGBank(bank, address);
}

void Mapper065::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[GetCHRAddress(address)] = value;
}

Byte Mapper065::ReadCHR(Address address) {
  const size_t index = GetCHRAddress(address);
  return uses_character_ram_ ? character_ram_[index] : rom_data()->CHR[index];
}

uint32_t Mapper065::GetAbsoluteCHRAddress(Address address) {
  return static_cast<uint32_t>(GetCHRAddress(address));
}

NametableMirroring Mapper065::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper065::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper065::M2CycleIRQ() {
  if (!irq_enabled_ || irq_counter_ == 0)
    return;

  --irq_counter_;
  if (irq_counter_ == 0) {
    irq_enabled_ = false;
    if (irq_callback())
      irq_callback().Run();
  }
}

void Mapper065::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_banks_)
      .WriteData(chr_banks_)
      .WriteData(prg_mode_)
      .WriteData(irq_enabled_)
      .WriteData(irq_counter_)
      .WriteData(irq_reload_value_)
      .WriteData(mirroring_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper065::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_banks_)
      .ReadData(&chr_banks_)
      .ReadData(&prg_mode_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_reload_value_)
      .ReadData(&mirroring_);
  if (uses_character_ram_)
    data.ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper065::ResetRegisters() {
  prg_banks_ = {0, 1};
  chr_banks_.fill(0);
  prg_mode_ = false;
  irq_enabled_ = false;
  irq_counter_ = 0;
  irq_reload_value_ = 0;
  mirroring_ = rom_data()->name_table_mirroring;
}

size_t Mapper065::GetCHRAddress(Address address) const {
  const size_t bank = chr_banks_[(address >> 10) & 0x07] % chr_bank_count_;
  return bank * kCHRBankSize + (address & 0x03ff);
}

Byte Mapper065::ReadPRGBank(size_t bank, Address address) {
  const size_t index =
      (bank % prg_bank_count_) * kPRGBankSize + (address & 0x1fff);
  return rom_data()->PRG[index];
}

}  // namespace nes
}  // namespace kiwi
