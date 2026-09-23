// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper206.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kCHR2KBankSize = 0x0800;
constexpr size_t kCHRRAMSize = 0x2000;
constexpr size_t kFourScreenRAMSize = 0x1000;

}  // namespace

Mapper206::Mapper206(Cartridge* cartridge) : Mapper(cartridge) {
  switch (rom_data()->mapper) {
    case 76:
      variant_ = Variant::kNamco3446;
      break;
    case 88:
      variant_ = Variant::kNamco108_88;
      break;
    case 95:
      variant_ = Variant::kNamco3425;
      break;
    case 154:
      variant_ = Variant::kNamco3453;
      break;
    default:
      DCHECK_EQ(rom_data()->mapper, 206);
      break;
  }

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

  if (rom_data()->name_table_mirroring == NametableMirroring::kFourScreen)
    four_screen_ram_.resize(kFourScreenRAMSize);
  ResetRegisters();
}

Mapper206::~Mapper206() = default;

void Mapper206::Reset() {
  ResetRegisters();
}

void Mapper206::WritePRG(Address address, Byte value) {
  if (variant_ == Variant::kNamco3453) {
    const NametableMirroring mirroring =
        value & 0x40 ? NametableMirroring::kOneScreenHigher
                     : NametableMirroring::kOneScreenLower;
    if (mirroring_ != mirroring) {
      mirroring_ = mirroring;
      if (mirroring_changed_callback())
        mirroring_changed_callback().Run();
    }
  }

  if (address < 0x8000 || address >= 0xa000)
    return;
  if ((address & 0x01) == 0) {
    selected_register_ = value & 0x07;
    return;
  }

  bank_registers_[selected_register_] = value;
  if (variant_ == Variant::kNamco3425) {
    nametable_pages_[0] = (bank_registers_[0] >> 5) & 0x01;
    nametable_pages_[1] = (bank_registers_[1] >> 5) & 0x01;
  }
}

Byte Mapper206::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  if (rom_data()->mapper == 206 && rom_data()->submapper == 1 &&
      rom_data()->PRG.size() == 0x8000) {
    return rom_data()->PRG[address - 0x8000];
  }

  const size_t window = (address - 0x8000) / kPRGBankSize;
  Byte bank = 0;
  switch (window) {
    case 0:
      bank = bank_registers_[6] & 0x0f;
      break;
    case 1:
      bank = bank_registers_[7] & 0x0f;
      break;
    case 2:
      bank = static_cast<Byte>(prg_bank_count_ - 2);
      break;
    default:
      bank = static_cast<Byte>(prg_bank_count_ - 1);
      break;
  }
  return ReadPRGBank(bank, address);
}

void Mapper206::WriteCHR(Address address, Byte value) {
  if (address < 0x2000) {
    if (uses_character_ram_)
      WriteCHRMemory(address, value);
    return;
  }
  if (address < 0x3000 && !four_screen_ram_.empty())
    four_screen_ram_[address - 0x2000] = value;
}

Byte Mapper206::ReadCHR(Address address) {
  if (address < 0x2000)
    return ReadCHRMemory(address);
  if (address < 0x3000 && !four_screen_ram_.empty())
    return four_screen_ram_[address - 0x2000];
  return 0;
}

uint32_t Mapper206::GetAbsoluteCHRAddress(Address address) {
  return static_cast<uint32_t>(GetCHRAddress(address));
}

NametableMirroring Mapper206::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper206::UsesCustomPPUMemoryMapping() const {
  return variant_ == Variant::kNamco3425;
}

Byte Mapper206::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHRMemory(address);

  const Byte page = nametable_pages_[(address >> 11) & 0x01];
  return ciram[page * kCHRBankSize + (address & 0x03ff)];
}

void Mapper206::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    if (uses_character_ram_)
      WriteCHRMemory(address, value);
    return;
  }

  const Byte page = nametable_pages_[(address >> 11) & 0x01];
  ciram[page * kCHRBankSize + (address & 0x03ff)] = value;
}

void Mapper206::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_register_)
      .WriteData(bank_registers_)
      .WriteData(nametable_pages_)
      .WriteData(mirroring_);
  if (uses_character_ram_)
    data.WriteData(character_ram_);
  if (!four_screen_ram_.empty())
    data.WriteData(four_screen_ram_);
  Mapper::Serialize(data);
}

bool Mapper206::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_register_)
      .ReadData(&bank_registers_)
      .ReadData(&nametable_pages_)
      .ReadData(&mirroring_);
  if (uses_character_ram_)
    data.ReadData(&character_ram_);
  if (!four_screen_ram_.empty())
    data.ReadData(&four_screen_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper206::ResetRegisters() {
  selected_register_ = 0;
  constexpr Byte kInitialBanks[] = {0, 2, 4, 5, 6, 7, 0, 1};
  std::copy(std::begin(kInitialBanks), std::end(kInitialBanks),
            std::begin(bank_registers_));
  nametable_pages_[0] = 0;
  nametable_pages_[1] = 1;
  mirroring_ = rom_data()->name_table_mirroring;
}

size_t Mapper206::GetCHRAddress(Address address) const {
  if (variant_ == Variant::kNamco3446) {
    const size_t window = address / kCHR2KBankSize;
    const size_t bank_count = chr_bank_count_ / 2;
    const Byte bank = bank_registers_[window + 2] & 0x3f;
    return (static_cast<size_t>(bank) % bank_count) * kCHR2KBankSize +
           (address & 0x07ff);
  }

  const size_t window = address / kCHRBankSize;
  Byte bank = 0;
  if (window < 2) {
    bank = static_cast<Byte>((bank_registers_[0] & 0x3e) + window);
  } else if (window < 4) {
    bank = static_cast<Byte>((bank_registers_[1] & 0x3e) + (window - 2));
  } else {
    bank = bank_registers_[window - 2] & 0x3f;
  }
  if ((variant_ == Variant::kNamco108_88 || variant_ == Variant::kNamco3453) &&
      window >= 4) {
    bank |= 0x40;
  }
  return (static_cast<size_t>(bank) % chr_bank_count_) * kCHRBankSize +
         (address & 0x03ff);
}

Byte Mapper206::ReadCHRMemory(Address address) {
  const size_t index = GetCHRAddress(address);
  return uses_character_ram_ ? character_ram_[index] : rom_data()->CHR[index];
}

void Mapper206::WriteCHRMemory(Address address, Byte value) {
  character_ram_[GetCHRAddress(address)] = value;
}

Byte Mapper206::ReadPRGBank(Byte bank, Address address) {
  const size_t index =
      (static_cast<size_t>(bank) % prg_bank_count_) * kPRGBankSize +
      (address & 0x1fff);
  return rom_data()->PRG[index];
}

}  // namespace nes
}  // namespace kiwi
