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

#include "nes/mappers/mapper141.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x0800;
constexpr size_t kCHRRAMSize = 0x2000;
constexpr Address kRegisterMask = 0xc101;
constexpr Address kRegisterIndex = 0x4100;
constexpr Address kRegisterData = 0x4101;

}  // namespace

Mapper141::Mapper141(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 141);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK_GE(rom_data()->PRG.size(), kPRGBankSize);

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRRAMSize);
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    DCHECK_GE(rom_data()->CHR.size(), 4 * kCHRBankSize);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  Reset();
}

Mapper141::~Mapper141() = default;

void Mapper141::Reset() {
  current_register_ = 0;
  registers_.fill(0);
}

void Mapper141::WritePRG(Address address, Byte value) {}

Byte Mapper141::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = registers_[5] % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper141::WriteCHR(Address address, Byte value) {
  DCHECK_LT(address, 0x2000);
  if (!character_ram_.empty())
    character_ram_[address] = value;
}

Byte Mapper141::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[address];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper141::GetAbsoluteCHRAddress(Address address) {
  DCHECK_LT(address, 0x2000);
  if (!character_ram_.empty())
    return address;

  const size_t window = (address >> 11) & 0x03;
  const bool simple_mode = (registers_[7] & 0x01) != 0;
  const size_t register_bank = (static_cast<size_t>(registers_[4]) << 3) |
                               registers_[simple_mode ? 0 : window];
  const size_t bank =
      ((register_bank << 1) | (window & 0x01)) % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x07ff));
}

void Mapper141::WriteExtendedRAM(Address address, Byte value) {
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

NametableMirroring Mapper141::GetNametableMirroring() {
  switch (GetMirroringMode()) {
    case 0:
      return NametableMirroring::kVertical;
    case 3:
      return NametableMirroring::kOneScreenLower;
    default:
      return NametableMirroring::kHorizontal;
  }
}

bool Mapper141::UsesCustomPPUMemoryMapping() const {
  return true;
}

Byte Mapper141::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHR(address);
  return ciram[GetNametablePage(address) * 0x0400 + (address & 0x03ff)];
}

void Mapper141::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    WriteCHR(address, value);
    return;
  }
  ciram[GetNametablePage(address) * 0x0400 + (address & 0x03ff)] = value;
}

void Mapper141::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(current_register_).WriteData(registers_);
  if (!character_ram_.empty())
    data.WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper141::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&current_register_).ReadData(&registers_);
  if (!character_ram_.empty()) {
    CHECK_EQ(character_ram_.size(), kCHRRAMSize);
    data.ReadData(&character_ram_);
  }
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

Byte Mapper141::GetMirroringMode() const {
  return registers_[7] & 0x01 ? 0 : (registers_[7] >> 1) & 0x03;
}

Byte Mapper141::GetNametablePage(Address address) const {
  const Byte nametable = (address >> 10) & 0x03;
  switch (GetMirroringMode()) {
    case 0:
      return nametable & 0x01;
    case 1:
      return nametable >> 1;
    case 2:
      return nametable == 0 ? 0 : 1;
    default:
      return 0;
  }
}

}  // namespace nes
}  // namespace kiwi
