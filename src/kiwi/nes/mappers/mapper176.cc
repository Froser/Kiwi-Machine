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

#include "nes/mappers/mapper176.h"

#include <algorithm>

#include "base/check.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kCHRRAMSize = 0x40000;
constexpr size_t kWorkRAMBankSize = 0x2000;

}  // namespace

Mapper176::Mapper176(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 4 * kPRGBankSize);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;

  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  chr_rom_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  character_ram_.resize(kCHRRAMSize);
  chr_ram_bank_count_ = character_ram_.size() / kCHRBankSize;

  EnsurePRGRAM(4 * kWorkRAMBankSize);
  const size_t work_ram_size =
      rom_data()->prg_ram_size + rom_data()->prg_nvram_size;
  DCHECK_EQ(work_ram_size % kWorkRAMBankSize, 0u);
  work_ram_bank_count_ = work_ram_size / kWorkRAMBankSize;
  DCHECK_GT(work_ram_bank_count_, 0u);

  Reset();
}

Mapper176::~Mapper176() = default;

void Mapper176::Reset() {
  bank_registers_ = {0, 2, 4, 5, 6, 7, 0, 1, 0xfe, 0xff, 0xff, 0xff};
  prg_banking_mode_ = 0;
  outer_chr_bank_size_ = false;
  select_chr_ram_ = false;
  mmc3_chr_mode_ = true;
  cnrom_chr_mode_ = false;
  prg_base_bits_ =
      rom_data()->PRG.size() == 0x100000 && rom_data()->CHR.size() == 0x100000
          ? 0x20
          : 0;
  chr_base_bits_ = 0;
  extended_mmc3_mode_ = false;
  wram_bank_select_ = 0;
  ram_in_first_chr_bank_ = false;
  allow_one_screen_mirroring_ = false;
  outer_registers_enabled_ = false;
  wram_config_enabled_ = false;
  wram_enabled_ = false;
  wram_write_protected_ = false;
  invert_prg_a14_ = false;
  invert_chr_a12_ = false;
  selected_register_ = 0;
  cnrom_chr_register_ = 0;
  mirroring_register_ = 0;
  last_vram_address_ = 0;
  irq_reload_value_ = 0;
  irq_counter_ = 0;
  irq_reload_pending_ = false;
  irq_enabled_ = false;
  UpdateMirroring();
}

void Mapper176::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  if (cnrom_chr_mode_ && (address <= 0x9fff || address >= 0xc000))
    cnrom_chr_register_ = value & 0x03;

  switch (address & 0xe001) {
    case 0x8000:
      invert_prg_a14_ = (value & 0x40) != 0;
      invert_chr_a12_ = (value & 0x80) != 0;
      selected_register_ = value & 0x0f;
      break;
    case 0x8001: {
      const Byte selected =
          selected_register_ & (extended_mmc3_mode_ ? 0x0f : 0x07);
      if (selected < bank_registers_.size())
        bank_registers_[selected] = value;
      break;
    }
    case 0xa000:
      mirroring_register_ = value & 0x03;
      UpdateMirroring();
      break;
    case 0xa001:
      if (!(value & 0x20))
        value &= 0xc0;
      wram_bank_select_ = value & 0x03;
      ram_in_first_chr_bank_ = (value & 0x04) != 0;
      allow_one_screen_mirroring_ = (value & 0x08) != 0;
      wram_config_enabled_ = (value & 0x20) != 0;
      outer_registers_enabled_ = (value & 0x40) != 0;
      wram_write_protected_ = (value & 0x40) != 0;
      wram_enabled_ = (value & 0x80) != 0;
      UpdateMirroring();
      break;
    case 0xc000:
      irq_reload_value_ = value;
      break;
    case 0xc001:
      irq_counter_ = 0;
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

Byte Mapper176::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = GetPRGBank(address) % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x1fff)];
}

void Mapper176::WriteCHR(Address address, Byte value) {
  const size_t bank = GetCHRBank(address);
  if (UsesCHRRAM(bank))
    character_ram_[GetCHRAddress(address)] = value;
}

Byte Mapper176::ReadCHR(Address address) {
  const size_t bank = GetCHRBank(address);
  const size_t index = GetCHRAddress(address);
  return UsesCHRRAM(bank) ? character_ram_[index] : rom_data()->CHR[index];
}

uint32_t Mapper176::GetAbsoluteCHRAddress(Address address) {
  return static_cast<uint32_t>(GetCHRAddress(address));
}

void Mapper176::WriteExtendedRAM(Address address, Byte value) {
  if (address >= 0x5000 && address <= 0x5fff &&
      (outer_registers_enabled_ || !wram_config_enabled_) &&
      (address & 0x5010) == 0x5010) {
    WriteOuterRegister(address, value);
    return;
  }

  bool writable = false;
  if (wram_config_enabled_) {
    writable = address >= 0x4020 && address <= 0x7fff;
  } else {
    writable = wram_enabled_ && !wram_write_protected_ && address >= 0x6000 &&
               address <= 0x7fff;
  }
  if (!writable)
    return;

  Byte* work_ram = Mapper::GetExtendedRAMPointer();
  DCHECK(work_ram);
  const size_t index = GetWorkRAMAddress(address);
  if (work_ram[index] == value)
    return;
  work_ram[index] = value;
  if (index < rom_data()->prg_nvram_size)
    MarkPRGNVRAMDirty();
}

Byte Mapper176::ReadExtendedRAM(Address address) {
  const bool readable =
      wram_config_enabled_
          ? address >= 0x4020 && address <= 0x7fff
          : wram_enabled_ && address >= 0x6000 && address <= 0x7fff;
  if (!readable)
    return static_cast<Byte>(address >> 8);

  Byte* work_ram = Mapper::GetExtendedRAMPointer();
  DCHECK(work_ram);
  return work_ram[GetWorkRAMAddress(address)];
}

Byte* Mapper176::GetExtendedRAMPointer() {
  if ((!wram_config_enabled_ && !wram_enabled_) || work_ram_bank_count_ == 0)
    return nullptr;
  Byte* work_ram = Mapper::GetExtendedRAMPointer();
  return work_ram +
         (wram_bank_select_ % work_ram_bank_count_) * kWorkRAMBankSize;
}

NametableMirroring Mapper176::GetNametableMirroring() {
  return mirroring_;
}

void Mapper176::ScanlineIRQ(int scanline, bool render_enabled) {
  if (render_enabled && scanline < 240)
    ClockIRQCounter();
}

void Mapper176::PPUAddressChanged(Address address) {
  const bool previous_a12 = (last_vram_address_ & 0x1000) != 0;
  const bool current_a12 = (address & 0x1000) != 0;
  if (!previous_a12 && current_a12)
    ClockIRQCounter();
  last_vram_address_ = address;
}

void Mapper176::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(bank_registers_)
      .WriteData(prg_banking_mode_)
      .WriteData(outer_chr_bank_size_)
      .WriteData(select_chr_ram_)
      .WriteData(mmc3_chr_mode_)
      .WriteData(cnrom_chr_mode_)
      .WriteData(prg_base_bits_)
      .WriteData(chr_base_bits_)
      .WriteData(extended_mmc3_mode_)
      .WriteData(wram_bank_select_)
      .WriteData(ram_in_first_chr_bank_)
      .WriteData(allow_one_screen_mirroring_)
      .WriteData(outer_registers_enabled_)
      .WriteData(wram_config_enabled_)
      .WriteData(wram_enabled_)
      .WriteData(wram_write_protected_)
      .WriteData(invert_prg_a14_)
      .WriteData(invert_chr_a12_)
      .WriteData(selected_register_)
      .WriteData(cnrom_chr_register_)
      .WriteData(mirroring_register_)
      .WriteData(mirroring_)
      .WriteData(last_vram_address_)
      .WriteData(irq_reload_value_)
      .WriteData(irq_counter_)
      .WriteData(irq_reload_pending_)
      .WriteData(irq_enabled_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper176::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&bank_registers_)
      .ReadData(&prg_banking_mode_)
      .ReadData(&outer_chr_bank_size_)
      .ReadData(&select_chr_ram_)
      .ReadData(&mmc3_chr_mode_)
      .ReadData(&cnrom_chr_mode_)
      .ReadData(&prg_base_bits_)
      .ReadData(&chr_base_bits_)
      .ReadData(&extended_mmc3_mode_)
      .ReadData(&wram_bank_select_)
      .ReadData(&ram_in_first_chr_bank_)
      .ReadData(&allow_one_screen_mirroring_)
      .ReadData(&outer_registers_enabled_)
      .ReadData(&wram_config_enabled_)
      .ReadData(&wram_enabled_)
      .ReadData(&wram_write_protected_)
      .ReadData(&invert_prg_a14_)
      .ReadData(&invert_chr_a12_)
      .ReadData(&selected_register_)
      .ReadData(&cnrom_chr_register_)
      .ReadData(&mirroring_register_)
      .ReadData(&mirroring_)
      .ReadData(&last_vram_address_)
      .ReadData(&irq_reload_value_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_reload_pending_)
      .ReadData(&irq_enabled_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper176::WriteOuterRegister(Address address, Byte value) {
  switch (address & 0x03) {
    case 0:
      prg_banking_mode_ = value & 0x07;
      outer_chr_bank_size_ = (value & 0x10) != 0;
      select_chr_ram_ = (value & 0x20) != 0;
      mmc3_chr_mode_ = (value & 0x40) == 0;
      prg_base_bits_ =
          static_cast<uint16_t>((prg_base_bits_ & ~0x0180) |
                                ((value & 0x80) << 1) | ((value & 0x08) << 4));
      break;
    case 1:
      prg_base_bits_ =
          static_cast<uint16_t>((prg_base_bits_ & ~0x007f) | (value & 0x7f));
      break;
    case 2:
      prg_base_bits_ = static_cast<uint16_t>((prg_base_bits_ & ~0x0200) |
                                             ((value & 0x40) << 3));
      chr_base_bits_ = value;
      cnrom_chr_register_ = 0;
      break;
    case 3:
      extended_mmc3_mode_ = (value & 0x02) != 0;
      cnrom_chr_mode_ = (value & 0x44) != 0;
      break;
  }
}

void Mapper176::UpdateMirroring() {
  const Byte selector =
      mirroring_register_ & (allow_one_screen_mirroring_ ? 0x03 : 0x01);
  NametableMirroring mirroring = NametableMirroring::kVertical;
  switch (selector) {
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
}

void Mapper176::ClockIRQCounter() {
  if (irq_counter_ == 0 || irq_reload_pending_) {
    irq_counter_ = irq_reload_value_;
    irq_reload_pending_ = false;
  } else {
    --irq_counter_;
  }
  if (irq_counter_ == 0 && irq_enabled_ && irq_callback())
    irq_callback().Run();
}

size_t Mapper176::GetPRGBank(Address address) const {
  const size_t window = (address - 0x8000) / kPRGBankSize;
  if (prg_banking_mode_ == 3)
    return (static_cast<size_t>(prg_base_bits_) << 1) + (window & 0x01);
  if (prg_banking_mode_ == 4)
    return ((static_cast<size_t>(prg_base_bits_) & 0x0ffe) << 1) + window;

  const size_t swap = invert_prg_a14_ ? 2 : 0;
  if (extended_mmc3_mode_) {
    const size_t outer = static_cast<size_t>(prg_base_bits_) << 1;
    switch (window ^ (window == 1 || window == 3 ? 0 : swap)) {
      case 0:
        return bank_registers_[6] | outer;
      case 1:
        return bank_registers_[7] | outer;
      case 2:
        return bank_registers_[8] | outer;
      default:
        return bank_registers_[9] | outer;
    }
  }

  const Byte mode = std::min<Byte>(prg_banking_mode_, 2);
  const size_t inner_mask = 0x3f >> mode;
  const size_t outer = (static_cast<size_t>(prg_base_bits_) << 1) & ~inner_mask;
  size_t bank = 0;
  const size_t effective_window =
      window == 1 || window == 3 ? window : window ^ swap;
  switch (effective_window) {
    case 0:
      bank = bank_registers_[6];
      break;
    case 1:
      bank = bank_registers_[7];
      break;
    case 2:
      bank = 0xfe;
      break;
    default:
      bank = 0xff;
      break;
  }
  return (bank & inner_mask) | outer;
}

size_t Mapper176::GetCHRBank(Address address) const {
  const size_t window = (address >> 10) & 0x07;
  if (!mmc3_chr_mode_) {
    const Byte inner_mask =
        cnrom_chr_mode_ ? (outer_chr_bank_size_ ? 1 : 3) : 0;
    return static_cast<size_t>((cnrom_chr_register_ & inner_mask) |
                               chr_base_bits_)
               << 3 |
           window;
  }

  const size_t effective_window = window ^ (invert_chr_a12_ ? 4 : 0);
  if (extended_mmc3_mode_) {
    static constexpr Byte kRegisterForWindow[] = {0, 10, 1, 11, 2, 3, 4, 5};
    return bank_registers_[kRegisterForWindow[effective_window]] |
           (static_cast<size_t>(chr_base_bits_) << 3);
  }

  const size_t inner_mask = outer_chr_bank_size_ ? 0x7f : 0xff;
  const size_t outer = (static_cast<size_t>(chr_base_bits_) << 3) & ~inner_mask;
  size_t bank = 0;
  switch (effective_window) {
    case 0:
      bank = bank_registers_[0] & 0xfe;
      break;
    case 1:
      bank = bank_registers_[0] | 0x01;
      break;
    case 2:
      bank = bank_registers_[1] & 0xfe;
      break;
    case 3:
      bank = bank_registers_[1] | 0x01;
      break;
    default:
      bank = bank_registers_[effective_window - 2];
      break;
  }
  return (bank & inner_mask) | outer;
}

size_t Mapper176::GetCHRAddress(Address address) const {
  const size_t bank = GetCHRBank(address);
  const size_t bank_count =
      UsesCHRRAM(bank) ? chr_ram_bank_count_ : chr_rom_bank_count_;
  DCHECK_GT(bank_count, 0u);
  return (bank % bank_count) * kCHRBankSize + (address & 0x03ff);
}

size_t Mapper176::GetWorkRAMAddress(Address address) const {
  const size_t bank =
      address < 0x6000 ? (wram_bank_select_ + 1) & 0x03 : wram_bank_select_;
  return (bank % work_ram_bank_count_) * kWorkRAMBankSize + (address & 0x1fff);
}

bool Mapper176::UsesCHRRAM(size_t bank) const {
  return chr_rom_bank_count_ == 0 || (mmc3_chr_mode_ && select_chr_ram_) ||
         (wram_config_enabled_ && ram_in_first_chr_bank_ && bank <= 7);
}

}  // namespace nes
}  // namespace kiwi
