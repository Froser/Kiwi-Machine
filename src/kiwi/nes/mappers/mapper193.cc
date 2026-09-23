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

#include "nes/mappers/mapper193.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x0800;

}  // namespace

Mapper193::Mapper193(Cartridge* cartridge)
    : Mapper(cartridge), mirroring_(rom_data()->name_table_mirroring) {
  DCHECK_EQ(rom_data()->mapper, 193);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 4 * kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper193::~Mapper193() = default;

void Mapper193::Reset() {
  selected_prg_bank_ = 0;
  chr_banks_ = {0, 1, 2, 3};
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper193::WritePRG(Address address, Byte value) {}

Byte Mapper193::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t slot = (address - 0x8000) / kPRGBankSize;
  const size_t bank = slot == 0 ? selected_prg_bank_ % prg_bank_count_
                                : prg_bank_count_ - 4 + slot;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x1fff)];
}

void Mapper193::WriteCHR(Address address, Byte value) {}

Byte Mapper193::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper193::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t slot = address / kCHRBankSize;
  const size_t bank = chr_banks_[slot] % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x07ff));
}

void Mapper193::WriteExtendedRAM(Address address, Byte value) {
  if (address < 0x6000 || address > 0x7fff)
    return;

  switch (address & 0x07) {
    case 0: {
      const Byte bank = (value & 0xfc) >> 1;
      chr_banks_[0] = bank;
      chr_banks_[1] = bank + 1;
      break;
    }
    case 1:
      chr_banks_[2] = value >> 1;
      break;
    case 2:
      chr_banks_[3] = value >> 1;
      break;
    case 3:
      selected_prg_bank_ = value;
      break;
    case 4:
      SetMirroring(value & 0x01 ? NametableMirroring::kHorizontal
                                : NametableMirroring::kVertical);
      break;
    default:
      break;
  }
}

NametableMirroring Mapper193::GetNametableMirroring() {
  return mirroring_;
}

void Mapper193::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(chr_banks_)
      .WriteData(mirroring_);
  Mapper::Serialize(data);
}

bool Mapper193::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&chr_banks_)
      .ReadData(&mirroring_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper193::SetMirroring(NametableMirroring mirroring) {
  if (mirroring_ == mirroring)
    return;
  mirroring_ = mirroring;
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
}

}  // namespace nes
}  // namespace kiwi
