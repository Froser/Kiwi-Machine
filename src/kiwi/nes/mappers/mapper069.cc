// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper069.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kWorkRAMSize = 0x8000;
constexpr size_t kCHRRAMSize = 0x2000;

}  // namespace

Mapper069::Mapper069(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  EnsurePRGRAM(kWorkRAMSize);

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

Mapper069::~Mapper069() = default;

void Mapper069::Reset() {
  ResetRegisters();
}

void Mapper069::WritePRG(Address address, Byte value) {
  switch (address & 0xe000) {
    case 0x8000:
      command_ = value & 0x0f;
      break;
    case 0xa000:
      WriteCommandParameter(value);
      break;
    case 0xc000:
    case 0xe000:
      // Sunsoft 5B expansion audio is not connected to Kiwi's APU yet.
      break;
  }
}

Byte Mapper069::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t window = (address - 0x8000) / kPRGBankSize;
  const Byte bank =
      window < 3 ? prg_banks_[window]
                 : static_cast<Byte>(rom_data()->PRG.size() / kPRGBankSize - 1);
  return ReadPRGBank(bank, address);
}

void Mapper069::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper069::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper069::GetAbsoluteCHRAddress(Address address) {
  if (uses_character_ram_)
    return address & 0x1fff;

  const size_t bank = chr_banks_[(address >> 10) & 0x07] % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x03ff));
}

void Mapper069::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 || address > 0x7fff)
    return;
  if ((work_ram_value_ & 0xc0) != 0xc0)
    return;

  const size_t ram_size = rom_data()->prg_ram_size + rom_data()->prg_nvram_size;
  const size_t index =
      ((work_ram_value_ & 0x3f) * kPRGBankSize + (address & 0x1fff)) % ram_size;
  Byte* ram = GetExtendedRAMPointer();
  DCHECK(ram);
  if (ram[index] != value) {
    ram[index] = value;
    if (index < rom_data()->prg_nvram_size)
      MarkPRGNVRAMDirty();
  }
}

Byte Mapper069::ReadExtendedRAM(Address address) {
  if (address < 0x6000 || address > 0x7fff)
    return Mapper::ReadExtendedRAM(address);

  if (!(work_ram_value_ & 0x40))
    return ReadPRGBank(work_ram_value_ & 0x3f, address);
  if (!(work_ram_value_ & 0x80))
    return static_cast<Byte>(address >> 8);

  const size_t ram_size = rom_data()->prg_ram_size + rom_data()->prg_nvram_size;
  const size_t index =
      ((work_ram_value_ & 0x3f) * kPRGBankSize + (address & 0x1fff)) % ram_size;
  Byte* ram = GetExtendedRAMPointer();
  DCHECK(ram);
  return ram[index];
}

NametableMirroring Mapper069::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper069::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper069::M2CycleIRQ() {
  if (!irq_counter_enabled_)
    return;

  --irq_counter_;
  if (irq_counter_ == 0xffff && irq_enabled_ && irq_callback())
    irq_callback().Run();
}

void Mapper069::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(command_)
      .WriteData(work_ram_value_)
      .WriteData(prg_banks_)
      .WriteData(chr_banks_)
      .WriteData(irq_counter_)
      .WriteData(irq_enabled_)
      .WriteData(irq_counter_enabled_)
      .WriteData(mirroring_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper069::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&command_)
      .ReadData(&work_ram_value_)
      .ReadData(&prg_banks_)
      .ReadData(&chr_banks_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_counter_enabled_)
      .ReadData(&mirroring_);
  if (uses_character_ram_)
    data.ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper069::ResetRegisters() {
  command_ = 0;
  work_ram_value_ = 0;
  std::fill(std::begin(prg_banks_), std::end(prg_banks_), 0);
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  irq_counter_ = 0;
  irq_enabled_ = false;
  irq_counter_enabled_ = false;
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper069::WriteCommandParameter(Byte value) {
  if (command_ <= 0x07) {
    chr_banks_[command_] = value;
    return;
  }
  if (command_ >= 0x09 && command_ <= 0x0b) {
    prg_banks_[command_ - 0x09] = value & 0x3f;
    return;
  }

  switch (command_) {
    case 0x08:
      work_ram_value_ = value;
      break;
    case 0x0c: {
      NametableMirroring mirroring = NametableMirroring::kVertical;
      switch (value & 0x03) {
        case 0:
          mirroring = NametableMirroring::kVertical;
          break;
        case 1:
          mirroring = NametableMirroring::kHorizontal;
          break;
        case 2:
          mirroring = NametableMirroring::kOneScreenLower;
          break;
        case 3:
          mirroring = NametableMirroring::kOneScreenHigher;
          break;
      }
      if (mirroring_ != mirroring) {
        mirroring_ = mirroring;
        if (mirroring_changed_callback())
          mirroring_changed_callback().Run();
      }
      break;
    }
    case 0x0d:
      irq_enabled_ = (value & 0x01) != 0;
      irq_counter_enabled_ = (value & 0x80) != 0;
      break;
    case 0x0e:
      irq_counter_ = static_cast<uint16_t>((irq_counter_ & 0xff00) | value);
      break;
    case 0x0f:
      irq_counter_ =
          static_cast<uint16_t>((irq_counter_ & 0x00ff) | (value << 8));
      break;
  }
}

Byte Mapper069::ReadPRGBank(Byte bank, Address offset) {
  const size_t index =
      static_cast<size_t>(bank) * kPRGBankSize + (offset & 0x1fff);
  return rom_data()->PRG[index % rom_data()->PRG.size()];
}

}  // namespace nes
}  // namespace kiwi
