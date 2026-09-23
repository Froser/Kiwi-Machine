// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper150.h"

#include <algorithm>
#include <iterator>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kCHRBankSize = 0x2000;
constexpr Address kRegisterMask = 0xc101;
constexpr Address kRegisterIndex = 0x4100;
constexpr Address kRegisterData = 0x4101;

}  // namespace

Mapper150::Mapper150(Cartridge* cartridge) : Mapper(cartridge) {
  DCHECK(rom_data()->mapper == 150 || rom_data()->mapper == 243);
  DCHECK(!rom_data()->PRG.empty());

  if (rom_data()->CHR.empty()) {
    character_ram_.resize(kCHRBankSize);
    chr_bank_count_ = 1;
  } else {
    DCHECK_EQ(rom_data()->CHR.size() % kCHRBankSize, 0u);
    chr_bank_count_ = rom_data()->CHR.size() / kCHRBankSize;
  }
  DCHECK_GT(chr_bank_count_, 0u);
  Reset();
}

Mapper150::~Mapper150() = default;

void Mapper150::Reset() {
  current_register_ = 0;
  std::fill(std::begin(registers_), std::end(registers_), 0);
  UpdateState();
}

void Mapper150::WritePRG(Address address, Byte value) {}

Byte Mapper150::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t index = static_cast<size_t>(selected_prg_bank_) * kPRGBankSize +
                       (address & 0x7fff);
  return rom_data()->PRG[index % rom_data()->PRG.size()];
}

void Mapper150::WriteCHR(Address address, Byte value) {
  if (!character_ram_.empty())
    character_ram_[address & 0x1fff] = value;
}

Byte Mapper150::ReadCHR(Address address) {
  if (!character_ram_.empty())
    return character_ram_[address & 0x1fff];
  return rom_data()->CHR[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper150::GetAbsoluteCHRAddress(Address address) {
  const size_t bank = selected_chr_bank_ % chr_bank_count_;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x1fff));
}

void Mapper150::WriteExtendedRAM(Address address, Byte value) {
  switch (address & kRegisterMask) {
    case kRegisterIndex:
      current_register_ = value & 0x07;
      return;
    case kRegisterData:
      registers_[current_register_] = value & 0x07;
      UpdateState();
      return;
    default:
      Mapper::WriteExtendedRAM(address, value);
  }
}

Byte Mapper150::ReadExtendedRAM(Address address) {
  if ((address & kRegisterMask) == kRegisterData) {
    return static_cast<Byte>((Mapper::ReadExtendedRAM(address) & 0xf8) |
                             registers_[current_register_]);
  }
  return Mapper::ReadExtendedRAM(address);
}

NametableMirroring Mapper150::GetNametableMirroring() {
  switch (mirroring_mode_) {
    case 1:
      return NametableMirroring::kHorizontal;
    case 2:
      return NametableMirroring::kVertical;
    case 3:
      return NametableMirroring::kOneScreenHigher;
    default:
      return NametableMirroring::kHorizontal;
  }
}

bool Mapper150::UsesCustomPPUMemoryMapping() const {
  return true;
}

Byte Mapper150::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHR(address);
  return ciram[GetNametablePage(address) * 0x0400 + (address & 0x03ff)];
}

void Mapper150::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    WriteCHR(address, value);
    return;
  }
  ciram[GetNametablePage(address) * 0x0400 + (address & 0x03ff)] = value;
}

void Mapper150::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(current_register_)
      .WriteData(registers_)
      .WriteData(selected_prg_bank_)
      .WriteData(selected_chr_bank_)
      .WriteData(mirroring_mode_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper150::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&current_register_)
      .ReadData(&registers_)
      .ReadData(&selected_prg_bank_)
      .ReadData(&selected_chr_bank_)
      .ReadData(&mirroring_mode_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

void Mapper150::UpdateState() {
  selected_prg_bank_ = registers_[5] & 0x03;
  if (rom_data()->mapper == 150) {
    selected_chr_bank_ = static_cast<Byte>(((registers_[4] & 0x01) << 2) |
                                           (registers_[6] & 0x03));
  } else {
    selected_chr_bank_ = static_cast<Byte>((registers_[2] & 0x01) |
                                           ((registers_[4] & 0x01) << 1) |
                                           ((registers_[6] & 0x03) << 2));
  }

  const Byte mirroring_mode = (registers_[7] >> 1) & 0x03;
  if (mirroring_mode_ != mirroring_mode) {
    mirroring_mode_ = mirroring_mode;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

Byte Mapper150::GetNametablePage(Address address) const {
  const Byte nametable = (address >> 10) & 0x03;
  switch (mirroring_mode_) {
    case 0:
      return nametable == 3 ? 1 : 0;
    case 1:
      return nametable >> 1;
    case 2:
      return nametable & 0x01;
    default:
      return 1;
  }
}

}  // namespace nes
}  // namespace kiwi
