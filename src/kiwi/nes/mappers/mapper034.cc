// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper034.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x1000;
constexpr size_t kWorkRAMSize = 0x2000;

}  // namespace

Mapper034::Mapper034(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 34);
  DCHECK_LE(rom_data()->submapper, 2);

  if (rom_data()->submapper == 1) {
    board_ = Board::kNINA001;
  } else if (rom_data()->submapper == 2) {
    board_ = Board::kBNROM;
  } else {
    board_ = rom_data()->CHR.empty() ? Board::kBNROM : Board::kNINA001;
  }

  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK(!rom_data()->PRG.empty());
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = kCHRRAMSize / kCHRBankSize;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  if (board_ == Board::kNINA001)
    EnsurePRGRAM(kWorkRAMSize);

  Reset();
}

Mapper034::~Mapper034() = default;

void Mapper034::Reset() {
  selected_prg_bank_ = 0;
  selected_chr_banks_[0] = 0;
  selected_chr_banks_[1] = 0;
}

void Mapper034::WritePRG(Address address, Byte value) {
  if (board_ != Board::kBNROM || address < 0x8000)
    return;

  // The BNROM register and PRG-ROM both drive the CPU data bus.
  selected_prg_bank_ = value & ReadPRG(address);
}

Byte Mapper034::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = selected_prg_bank_ % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper034::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper034::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[GetAbsoluteCHRAddress(address)];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper034::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  if (board_ == Board::kBNROM)
    return address & 0x1fff;

  const size_t window = (address >> 12) & 0x01;
  const size_t bank = selected_chr_banks_[window] % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x0fff));
}

void Mapper034::WriteExtendedRAM(Address address, Byte value) {
  if (board_ == Board::kNINA001) {
    switch (address) {
      case 0x7ffd:
        selected_prg_bank_ = value;
        break;
      case 0x7ffe:
        selected_chr_banks_[0] = value;
        break;
      case 0x7fff:
        selected_chr_banks_[1] = value;
        break;
    }
  }

  Mapper::WriteExtendedRAM(address, value);
}

void Mapper034::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(selected_chr_banks_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper034::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_banks_)
      .ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
