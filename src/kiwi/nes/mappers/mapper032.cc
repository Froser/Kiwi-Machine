// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper032.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0400;

}  // namespace

Mapper032::Mapper032(Cartridge* cartridge)
    : Mapper(cartridge),
      is_major_league_board_(rom_data()->submapper == 1),
      mirroring_(rom_data()->name_table_mirroring) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GE(prg_bank_count_, 2u);

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
    chr_bank_count_ = character_ram_.size() / kCHRBankSize;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);

  Reset();
}

Mapper032::~Mapper032() = default;

void Mapper032::Reset() {
  std::fill(std::begin(prg_banks_), std::end(prg_banks_), 0);
  std::fill(std::begin(chr_banks_), std::end(chr_banks_), 0);
  prg_mode_ = false;
  mirroring_ = is_major_league_board_ ? NametableMirroring::kOneScreenHigher
                                      : rom_data()->name_table_mirroring;
}

void Mapper032::WritePRG(Address address, Byte value) {
  switch (address & 0xf000) {
    case 0x8000:
      prg_banks_[0] = value & 0x1f;
      break;
    case 0x9000: {
      if (is_major_league_board_)
        return;

      prg_mode_ = value & 0x02;
      const NametableMirroring mirroring = value & 0x01
                                               ? NametableMirroring::kHorizontal
                                               : NametableMirroring::kVertical;
      if (mirroring_ != mirroring) {
        mirroring_ = mirroring;
        if (mirroring_changed_callback())
          mirroring_changed_callback().Run();
      }
      break;
    }
    case 0xa000:
      prg_banks_[1] = value & 0x1f;
      break;
    case 0xb000:
      chr_banks_[address & 0x07] = value;
      break;
    default:
      break;
  }
}

Byte Mapper032::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t window = (address - 0x8000) / kPRGBankSize;
  size_t bank = prg_bank_count_ - 1;
  switch (window) {
    case 0:
      bank = prg_mode_ ? prg_bank_count_ - 2 : prg_banks_[0];
      break;
    case 1:
      bank = prg_banks_[1];
      break;
    case 2:
      bank = prg_mode_ ? prg_banks_[0] : prg_bank_count_ - 2;
      break;
    default:
      break;
  }
  return rom_data()
      ->PRG[(bank % prg_bank_count_) * kPRGBankSize + (address & 0x1fff)];
}

void Mapper032::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper032::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[GetAbsoluteCHRAddress(address)];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper032::GetAbsoluteCHRAddress(Address address) {
  const size_t bank = chr_banks_[(address >> 10) & 0x07] % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x03ff));
}

NametableMirroring Mapper032::GetNametableMirroring() {
  return mirroring_;
}

void Mapper032::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_banks_)
      .WriteData(chr_banks_)
      .WriteData(prg_mode_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper032::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_banks_)
      .ReadData(&chr_banks_)
      .ReadData(&prg_mode_)
      .ReadData(&mirroring_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
