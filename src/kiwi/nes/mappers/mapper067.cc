// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper067.h"

#include <algorithm>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x0800;
constexpr size_t kCHRRAMSize = 0x2000;

}  // namespace

Mapper067::Mapper067(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 67);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  uses_character_ram_ = rom_data()->CHR.empty();
  if (uses_character_ram_) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = kCHRRAMSize / kCHRBankSize;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  ResetRegisters();
}

Mapper067::~Mapper067() = default;

void Mapper067::Reset() {
  ResetRegisters();
}

void Mapper067::WritePRG(Address address, Byte value) {
  switch (address & 0xf800) {
    case 0x8800:
    case 0x9800:
    case 0xa800:
    case 0xb800:
      chr_banks_[(address >> 12) - 8] = value;
      break;
    case 0xc800:
      if (irq_write_low_) {
        irq_counter_ = static_cast<uint16_t>((irq_counter_ & 0xff00) | value);
      } else {
        irq_counter_ =
            static_cast<uint16_t>((irq_counter_ & 0x00ff) | (value << 8));
      }
      irq_write_low_ = !irq_write_low_;
      break;
    case 0xd800:
      irq_enabled_ = (value & 0x10) != 0;
      irq_write_low_ = false;
      break;
    case 0xe800:
      SetMirroring(value);
      break;
    case 0xf800:
      prg_bank_ = value;
      break;
  }
}

Byte Mapper067::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank =
      address < 0xc000 ? prg_bank_ : static_cast<Byte>(prg_bank_count_ - 1);
  return ReadPRGBank(bank, address);
}

void Mapper067::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[GetCHRAddress(address)] = value;
}

Byte Mapper067::ReadCHR(Address address) {
  const size_t index = GetCHRAddress(address);
  return uses_character_ram_ ? character_ram_[index] : rom_data()->CHR[index];
}

uint32_t Mapper067::GetAbsoluteCHRAddress(Address address) {
  return static_cast<uint32_t>(GetCHRAddress(address));
}

NametableMirroring Mapper067::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper067::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper067::M2CycleIRQ() {
  if (!irq_enabled_)
    return;

  --irq_counter_;
  if (irq_counter_ == 0xffff) {
    irq_enabled_ = false;
    if (irq_callback())
      irq_callback().Run();
  }
}

void Mapper067::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_bank_)
      .WriteData(chr_banks_)
      .WriteData(irq_write_low_)
      .WriteData(irq_enabled_)
      .WriteData(irq_counter_)
      .WriteData(mirroring_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper067::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_bank_)
      .ReadData(&chr_banks_)
      .ReadData(&irq_write_low_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_counter_)
      .ReadData(&mirroring_);
  if (uses_character_ram_)
    data.ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper067::ResetRegisters() {
  prg_bank_ = 0;
  chr_banks_.fill(0);
  irq_write_low_ = false;
  irq_enabled_ = false;
  irq_counter_ = 0;
  mirroring_ = rom_data()->name_table_mirroring;
}

size_t Mapper067::GetCHRAddress(Address address) const {
  const size_t window = (address >> 11) & 0x03;
  const size_t bank = chr_banks_[window] % chr_bank_count_;
  return bank * kCHRBankSize + (address & 0x07ff);
}

Byte Mapper067::ReadPRGBank(size_t bank, Address address) {
  const size_t index =
      (bank % prg_bank_count_) * kPRGBankSize + (address & 0x3fff);
  return rom_data()->PRG[index];
}

void Mapper067::SetMirroring(Byte value) {
  constexpr std::array<NametableMirroring, 4> kMirroringModes{{
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
  }};
  const NametableMirroring mirroring = kMirroringModes[value & 0x03];
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

}  // namespace nes
}  // namespace kiwi
