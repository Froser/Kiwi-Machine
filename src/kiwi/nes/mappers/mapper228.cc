// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper228.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRBankSize = 0x2000;

}  // namespace

Mapper228::Mapper228(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 228);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), 2 * kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK(!rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper228::~Mapper228() = default;

void Mapper228::Reset() {
  SelectBanks(0x8000, 0);
}

void Mapper228::WritePRG(Address address, Byte value) {
  DCHECK_GE(address, 0x8000);
  SelectBanks(address, value);
}

Byte Mapper228::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  if (prg_open_bus_)
    return static_cast<Byte>(address >> 8);

  const size_t window = (address >> 14) & 0x01;
  const size_t bank = prg_banks_[window] % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper228::WriteCHR(Address address, Byte value) {}

Byte Mapper228::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper228::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  const size_t bank = chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + address);
}

void Mapper228::WriteExtendedRAM(Address address, Byte value) {}

NametableMirroring Mapper228::GetNametableMirroring() {
  return mirroring_;
}

void Mapper228::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_banks_)
      .WriteData(chr_bank_)
      .WriteData(mirroring_)
      .WriteData(prg_open_bus_);
}

bool Mapper228::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_banks_)
      .ReadData(&chr_bank_)
      .ReadData(&mirroring_)
      .ReadData(&prg_open_bus_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return true;
}

void Mapper228::SelectBanks(Address address, Byte value) {
  Byte chip = static_cast<Byte>((address >> 11) & 0x03);
  prg_open_bus_ = chip == 2;
  if (chip == 3)
    chip = 2;

  const Byte page = static_cast<Byte>(((address >> 6) & 0x1f) | (chip << 5));
  if (address & 0x20) {
    prg_banks_[0] = page;
    prg_banks_[1] = page;
  } else {
    prg_banks_[0] = page & 0xfe;
    prg_banks_[1] = static_cast<Byte>((page & 0xfe) + 1);
  }
  chr_bank_ = static_cast<Byte>(((address & 0x0f) << 2) | (value & 0x03));

  const NametableMirroring mirroring = address & 0x2000
                                           ? NametableMirroring::kHorizontal
                                           : NametableMirroring::kVertical;
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

}  // namespace nes
}  // namespace kiwi
