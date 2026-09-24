// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper168.h"

#include <algorithm>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x1000;
constexpr size_t kCHRRAMSize = 0x10000;
constexpr size_t kLegacyCHRNVRAMSize = 0x8000;
constexpr uint16_t kIRQCounterMask = 0x07ff;
constexpr uint16_t kIRQOutputBit = 0x0400;

size_t DecodeNES20RAMSize(Byte shift_count) {
  return shift_count == 0 ? 0 : static_cast<size_t>(64) << shift_count;
}

}  // namespace

Mapper168::Mapper168(Cartridge* cartridge)
    : Mapper(cartridge), character_ram_(kCHRRAMSize) {
  DCHECK_EQ(rom_data()->mapper, 168);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  std::copy_n(rom_data()->CHR.begin(),
              std::min(rom_data()->CHR.size(), character_ram_.size()),
              character_ram_.begin());

  if (rom_data()->is_nes_20 && rom_data()->raw_headers.size() > 11) {
    chr_nvram_size_ =
        DecodeNES20RAMSize(static_cast<Byte>(rom_data()->raw_headers[11] >> 4));
    chr_nvram_size_ = std::min(chr_nvram_size_, character_ram_.size());
  } else {
    chr_nvram_size_ = kLegacyCHRNVRAMSize;
  }

  // Mapper 168 has no CPU-visible PRG-RAM. Legacy headers use the battery bit
  // for the upper half of CHR-RAM, which travels through the existing save API.
  rom_data()->prg_ram_size = 0;
  rom_data()->prg_nvram_size = 0;
  rom_data()->has_battery = chr_nvram_size_ != 0;
  rom_data()->name_table_mirroring = NametableMirroring::kVertical;
  Reset();
}

Mapper168::~Mapper168() = default;

void Mapper168::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_bank_ = 0;
  irq_counter_ = 0;
  irq_disabled_ = false;
  irq_asserted_ = false;
  chr_nvram_unlocked_ = false;
}

void Mapper168::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  if (address < 0xc000) {
    selected_prg_bank_ = (value >> 6) & 0x03;
    selected_chr_bank_ = value & 0x0f;
    return;
  }

  WriteIRQControl(address, value);
}

Byte Mapper168::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = address < 0xc000 ? selected_prg_bank_ % prg_bank_count_
                                       : prg_bank_count_ - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper168::WriteCHR(Address address, Byte value) {
  const size_t index = GetAbsoluteCHRAddress(address);
  if (IsProtectedCHRAddress(index))
    return;

  if (character_ram_[index] == value)
    return;

  character_ram_[index] = value;
  if (chr_nvram_size_ != 0 &&
      index >= character_ram_.size() - chr_nvram_size_) {
    MarkPRGNVRAMDirty();
  }
}

Byte Mapper168::ReadCHR(Address address) {
  const size_t index = GetAbsoluteCHRAddress(address);
  if (IsProtectedCHRAddress(index))
    return static_cast<Byte>(address >> 8);
  return character_ram_[index];
}

uint32_t Mapper168::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = address < kCHRBankSize ? 0 : selected_chr_bank_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x0fff));
}

NametableMirroring Mapper168::GetNametableMirroring() {
  return NametableMirroring::kVertical;
}

bool Mapper168::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper168::M2CycleIRQ() {
  if (irq_disabled_)
    return;

  irq_counter_ = (irq_counter_ + 1) & kIRQCounterMask;
  const bool irq_asserted = (irq_counter_ & kIRQOutputBit) != 0;
  if (irq_asserted && !irq_asserted_ && irq_callback())
    irq_callback().Run();
  irq_asserted_ = irq_asserted;
}

size_t Mapper168::GetCustomNVRAMSize() const {
  return chr_nvram_size_;
}

void Mapper168::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(irq_counter_)
      .WriteData(irq_disabled_)
      .WriteData(irq_asserted_)
      .WriteData(chr_nvram_unlocked_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper168::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  const Bytes previous_nvram = CopyPRGNVRAM();
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_disabled_)
      .ReadData(&irq_asserted_)
      .ReadData(&chr_nvram_unlocked_)
      .ReadData(&character_ram_);
  if (previous_nvram != CopyPRGNVRAM())
    MarkPRGNVRAMDirty();
  return Mapper::Deserialize(header, data);
}

Bytes Mapper168::CopyPRGNVRAM() {
  if (chr_nvram_size_ == 0)
    return {};
  return Bytes(character_ram_.end() - chr_nvram_size_, character_ram_.end());
}

bool Mapper168::RestorePRGNVRAM(const Bytes& data) {
  if (data.size() != chr_nvram_size_)
    return false;
  std::copy(data.begin(), data.end(), character_ram_.end() - chr_nvram_size_);
  return true;
}

bool Mapper168::IsProtectedCHRAddress(size_t address) const {
  return !chr_nvram_unlocked_ && chr_nvram_size_ != 0 &&
         address >= character_ram_.size() - chr_nvram_size_;
}

void Mapper168::WriteIRQControl(Address address, Byte value) {
  const bool was_disabled = irq_disabled_;
  // Production boards select D2; A7 is accepted for unreworked PCB layouts.
  irq_disabled_ = (value & 0x04) != 0 || (address & 0x0080) != 0;
  if (irq_disabled_) {
    irq_counter_ = 0;
    irq_asserted_ = false;
  } else if (was_disabled) {
    chr_nvram_unlocked_ = true;
  }
}

}  // namespace nes
}  // namespace kiwi
