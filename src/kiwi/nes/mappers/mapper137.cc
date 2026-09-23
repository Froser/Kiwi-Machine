// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper137.h"

#include <algorithm>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x0400;
constexpr size_t kFixedCHRSize = 0x1000;
constexpr Address kRegisterMask = 0xc101;
constexpr Address kRegisterIndex = 0x4100;
constexpr Address kRegisterData = 0x4101;

}  // namespace

Mapper137::Mapper137(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 137);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);
  DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
  DCHECK_GE(rom_data()->CHR.size(), kFixedCHRSize);

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  Reset();
}

Mapper137::~Mapper137() = default;

void Mapper137::Reset() {
  current_register_ = 0;
  registers_.fill(0);
}

void Mapper137::WritePRG(Address address, Byte value) {}

Byte Mapper137::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = registers_[5] % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper137::WriteCHR(Address address, Byte value) {}

Byte Mapper137::ReadCHR(Address address) {
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper137::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  if (address >= 0x1000) {
    return static_cast<uint32_t>(rom_data()->CHR.size() - kFixedCHRSize +
                                 (address & 0x0fff));
  }

  const size_t window = (address >> 10) & 0x03;
  const bool simple_mode = (registers_[7] & 0x01) != 0;
  size_t bank = registers_[simple_mode ? 0 : window];
  switch (window) {
    case 1:
      bank |= (registers_[4] & 0x01) << 4;
      break;
    case 2:
      bank |= (registers_[4] & 0x02) << 3;
      break;
    case 3:
      bank |= ((registers_[4] & 0x04) << 2) | ((registers_[6] & 0x01) << 3);
      break;
  }
  bank %= chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x03ff));
}

void Mapper137::WriteExtendedRAM(Address address, Byte value) {
  switch (address & kRegisterMask) {
    case kRegisterIndex:
      current_register_ = value & 0x07;
      return;
    case kRegisterData: {
      const Byte previous_mirroring = GetMirroringMode();
      registers_[current_register_] = value & 0x07;
      if (previous_mirroring != GetMirroringMode() &&
          mirroring_changed_callback()) {
        mirroring_changed_callback().Run();
      }
      return;
    }
    default:
      Mapper::WriteExtendedRAM(address, value);
  }
}

NametableMirroring Mapper137::GetNametableMirroring() {
  switch (GetMirroringMode()) {
    case 1:
      return NametableMirroring::kVertical;
    case 3:
      return NametableMirroring::kOneScreenLower;
    default:
      return NametableMirroring::kHorizontal;
  }
}

bool Mapper137::UsesCustomPPUMemoryMapping() const {
  return true;
}

Byte Mapper137::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHR(address);
  return ciram[GetNametablePage(address) * 0x0400 + (address & 0x03ff)];
}

void Mapper137::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    WriteCHR(address, value);
    return;
  }
  ciram[GetNametablePage(address) * 0x0400 + (address & 0x03ff)] = value;
}

void Mapper137::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(current_register_).WriteData(registers_);
  Mapper::Serialize(data);
}

bool Mapper137::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&current_register_).ReadData(&registers_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

Byte Mapper137::GetMirroringMode() const {
  return registers_[7] & 0x01 ? 0 : (registers_[7] >> 1) & 0x03;
}

Byte Mapper137::GetNametablePage(Address address) const {
  const Byte nametable = (address >> 10) & 0x03;
  switch (GetMirroringMode()) {
    case 0:
      return nametable >> 1;
    case 1:
      return nametable & 0x01;
    case 2:
      return nametable == 0 ? 0 : 1;
    default:
      return 0;
  }
}

}  // namespace nes
}  // namespace kiwi
