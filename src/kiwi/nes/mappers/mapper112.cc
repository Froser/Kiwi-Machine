// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper112.h"

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

Mapper112::Mapper112(Cartridge* cartridge) : Mapper(cartridge) {
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

Mapper112::~Mapper112() = default;

void Mapper112::Reset() {
  current_register_ = 0;
  outer_chr_bank_ = 0;
  std::fill(std::begin(registers_), std::end(registers_), 0);
  mirroring_ = NametableMirroring::kVertical;
}

void Mapper112::WritePRG(Address address, Byte value) {
  switch (address & 0xe001) {
    case 0x8000:
      current_register_ = value & 0x07;
      break;
    case 0xa000:
      registers_[current_register_] = value;
      break;
    case 0xc000:
      outer_chr_bank_ = value;
      break;
    case 0xe000: {
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
    default:
      break;
  }
}

Byte Mapper112::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t window = (address - 0x8000) / kPRGBankSize;
  const size_t banks[] = {
      registers_[0],
      registers_[1],
      prg_bank_count_ - 2,
      prg_bank_count_ - 1,
  };
  const size_t bank = banks[window] % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x1fff)];
}

void Mapper112::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper112::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[GetAbsoluteCHRAddress(address)];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper112::GetAbsoluteCHRAddress(Address address) {
  const size_t bank = GetCHRBank(address) % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x03ff));
}

NametableMirroring Mapper112::GetNametableMirroring() {
  return mirroring_;
}

void Mapper112::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(current_register_)
      .WriteData(outer_chr_bank_)
      .WriteData(registers_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper112::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&current_register_)
      .ReadData(&outer_chr_bank_)
      .ReadData(&registers_)
      .ReadData(&mirroring_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

size_t Mapper112::GetCHRBank(Address address) const {
  const size_t window = (address & 0x1fff) / kCHRBankSize;
  if (window < 2)
    return static_cast<size_t>(registers_[2]) + window;
  if (window < 4)
    return static_cast<size_t>(registers_[3]) + window - 2;

  const Byte outer_bit = static_cast<Byte>(0x10 << (window - 4));
  return static_cast<size_t>(registers_[window]) |
         ((outer_chr_bank_ & outer_bit) ? 0x100 : 0);
}

}  // namespace nes
}  // namespace kiwi
