// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper071.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;

}  // namespace

Mapper071::Mapper071(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GT(prg_bank_count_, 0u);

  character_ram_.resize(kCHRRAMSize);
  Reset();
}

Mapper071::~Mapper071() = default;

void Mapper071::Reset() {
  selected_prg_bank_ = 0;
  uses_bf9097_mirroring_ = rom_data()->submapper == 1;
  mirroring_ = rom_data()->name_table_mirroring;
}

void Mapper071::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  if (address == 0x9000)
    uses_bf9097_mirroring_ = true;

  if (address >= 0xc000 || !uses_bf9097_mirroring_) {
    selected_prg_bank_ = value;
    return;
  }

  mirroring_ = (value & 0x10) ? NametableMirroring::kOneScreenLower
                              : NametableMirroring::kOneScreenHigher;
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
}

Byte Mapper071::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = address < 0xc000 ? selected_prg_bank_ % prg_bank_count_
                                       : prg_bank_count_ - 1;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper071::WriteCHR(Address address, Byte value) {
  character_ram_[address & 0x1fff] = value;
}

Byte Mapper071::ReadCHR(Address address) {
  return character_ram_[address & 0x1fff];
}

NametableMirroring Mapper071::GetNametableMirroring() {
  return mirroring_;
}

void Mapper071::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(selected_prg_bank_)
      .WriteData(uses_bf9097_mirroring_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper071::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&selected_prg_bank_)
      .ReadData(&uses_bf9097_mirroring_)
      .ReadData(&mirroring_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
