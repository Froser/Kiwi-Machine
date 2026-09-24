// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper018.h"

#include <algorithm>
#include <array>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kCHRRAMSize = 0x2000;
constexpr std::array<uint16_t, 4> kIRQMask{
    0xffff,
    0x0fff,
    0x00ff,
    0x000f,
};

}  // namespace

Mapper018::Mapper018(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GT(prg_bank_count_, 0u);

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

Mapper018::~Mapper018() = default;

void Mapper018::Reset() {
  ResetRegisters();
}

void Mapper018::WritePRG(Address address, Byte value) {
  const bool upper_nibble = (address & 0x01) != 0;
  value &= 0x0f;

  switch (address & 0xf003) {
    case 0x8000:
    case 0x8001:
      UpdatePRGBank(0, value, upper_nibble);
      break;
    case 0x8002:
    case 0x8003:
      UpdatePRGBank(1, value, upper_nibble);
      break;
    case 0x9000:
    case 0x9001:
      UpdatePRGBank(2, value, upper_nibble);
      break;
    case 0xa000:
    case 0xa001:
      UpdateCHRBank(0, value, upper_nibble);
      break;
    case 0xa002:
    case 0xa003:
      UpdateCHRBank(1, value, upper_nibble);
      break;
    case 0xb000:
    case 0xb001:
      UpdateCHRBank(2, value, upper_nibble);
      break;
    case 0xb002:
    case 0xb003:
      UpdateCHRBank(3, value, upper_nibble);
      break;
    case 0xc000:
    case 0xc001:
      UpdateCHRBank(4, value, upper_nibble);
      break;
    case 0xc002:
    case 0xc003:
      UpdateCHRBank(5, value, upper_nibble);
      break;
    case 0xd000:
    case 0xd001:
      UpdateCHRBank(6, value, upper_nibble);
      break;
    case 0xd002:
    case 0xd003:
      UpdateCHRBank(7, value, upper_nibble);
      break;
    case 0xe000:
    case 0xe001:
    case 0xe002:
    case 0xe003:
      irq_reload_[address & 0x03] = value;
      break;
    case 0xf000:
      irq_counter_ =
          static_cast<uint16_t>(irq_reload_[0] | (irq_reload_[1] << 4) |
                                (irq_reload_[2] << 8) | (irq_reload_[3] << 12));
      break;
    case 0xf001:
      irq_enabled_ = (value & 0x01) != 0;
      if (value & 0x08) {
        irq_counter_size_ = 3;
      } else if (value & 0x04) {
        irq_counter_size_ = 2;
      } else if (value & 0x02) {
        irq_counter_size_ = 1;
      } else {
        irq_counter_size_ = 0;
      }
      break;
    case 0xf002:
      switch (value & 0x03) {
        case 0:
          mirroring_ = NametableMirroring::kHorizontal;
          break;
        case 1:
          mirroring_ = NametableMirroring::kVertical;
          break;
        case 2:
          mirroring_ = NametableMirroring::kOneScreenLower;
          break;
        case 3:
          mirroring_ = NametableMirroring::kOneScreenHigher;
          break;
      }
      if (mirroring_changed_callback())
        mirroring_changed_callback().Run();
      break;
    case 0xf003:
      // The SS88006 PCM register is not connected to the base NES APU.
      break;
  }
}

Byte Mapper018::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);

  size_t bank = prg_bank_count_ - 1;
  if (address < 0xa000) {
    bank = prg_banks_[0] % prg_bank_count_;
  } else if (address < 0xc000) {
    bank = prg_banks_[1] % prg_bank_count_;
  } else if (address < 0xe000) {
    bank = prg_banks_[2] % prg_bank_count_;
  }

  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x1fff)];
}

void Mapper018::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper018::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address & 0x1fff];

  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper018::GetAbsoluteCHRAddress(Address address) {
  if (uses_character_ram_)
    return address & 0x1fff;

  const size_t bank = chr_banks_[(address >> 10) & 0x07] % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x03ff));
}

NametableMirroring Mapper018::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper018::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper018::M2CycleIRQ() {
  if (!irq_enabled_)
    return;

  const uint16_t mask = kIRQMask[irq_counter_size_];
  uint16_t counter = irq_counter_ & mask;
  --counter;
  irq_counter_ =
      static_cast<uint16_t>((irq_counter_ & ~mask) | (counter & mask));
  if ((counter & mask) == 0 && irq_callback())
    irq_callback().Run();
}

void Mapper018::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_banks_)
      .WriteData(chr_banks_)
      .WriteData(irq_reload_)
      .WriteData(irq_counter_)
      .WriteData(irq_counter_size_)
      .WriteData(irq_enabled_)
      .WriteData(mirroring_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper018::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_banks_)
      .ReadData(&chr_banks_)
      .ReadData(&irq_reload_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_counter_size_)
      .ReadData(&irq_enabled_)
      .ReadData(&mirroring_);
  if (uses_character_ram_)
    data.ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper018::ResetRegisters() {
  std::fill(std::begin(prg_banks_), std::end(prg_banks_), 0);
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  std::fill(std::begin(irq_reload_), std::end(irq_reload_), 0);
  irq_counter_ = 0;
  irq_counter_size_ = 0;
  irq_enabled_ = false;
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper018::UpdatePRGBank(size_t bank, Byte value, bool upper_nibble) {
  DCHECK_LT(bank, std::size(prg_banks_));
  if (upper_nibble) {
    prg_banks_[bank] =
        static_cast<Byte>((prg_banks_[bank] & 0x0f) | (value << 4));
  } else {
    prg_banks_[bank] = static_cast<Byte>((prg_banks_[bank] & 0xf0) | value);
  }
}

void Mapper018::UpdateCHRBank(size_t bank, Byte value, bool upper_nibble) {
  DCHECK_LT(bank, std::size(chr_banks_));
  if (upper_nibble) {
    chr_banks_[bank] =
        static_cast<Byte>((chr_banks_[bank] & 0x0f) | (value << 4));
  } else {
    chr_banks_[bank] = static_cast<Byte>((chr_banks_[bank] & 0xf0) | value);
  }
}

}  // namespace nes
}  // namespace kiwi
