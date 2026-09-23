// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper178.h"

#include <algorithm>

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x4000;
constexpr size_t kCHRRAMSize = 0x2000;
constexpr size_t kWorkRAMBankSize = 0x2000;
constexpr size_t kWorkRAMSize = 4 * kWorkRAMBankSize;

}  // namespace

Mapper178::Mapper178(Cartridge* cartridge)
    : Mapper(cartridge), mirroring_(rom_data()->name_table_mirroring) {
  DCHECK_EQ(rom_data()->mapper, 178);
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  DCHECK(!rom_data()->PRG.empty());
  DCHECK(rom_data()->CHR.empty());

  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  character_ram_.resize(kCHRRAMSize);

  EnsurePRGRAM(kWorkRAMSize);
  const size_t work_ram_size =
      rom_data()->prg_ram_size + rom_data()->prg_nvram_size;
  DCHECK_EQ(work_ram_size % kWorkRAMBankSize, 0u);
  work_ram_bank_count_ = work_ram_size / kWorkRAMBankSize;
  DCHECK_GT(work_ram_bank_count_, 0u);

  Reset();
}

Mapper178::~Mapper178() = default;

void Mapper178::Reset() {
  std::fill(std::begin(registers_), std::end(registers_), 0);
  UpdateMirroring();
}

void Mapper178::WritePRG(Address address, Byte value) {}

Byte Mapper178::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = GetPRGBank(address) % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x3fff)];
}

void Mapper178::WriteCHR(Address address, Byte value) {
  character_ram_[address & 0x1fff] = value;
}

Byte Mapper178::ReadCHR(Address address) {
  return character_ram_[address & 0x1fff];
}

uint32_t Mapper178::GetAbsoluteCHRAddress(Address address) {
  return address & 0x1fff;
}

void Mapper178::WriteExtendedRAM(Address address, Byte value) {
  if (address >= 0x4800 && address <= 0x4fff) {
    registers_[address & 0x03] = value;
    UpdateMirroring();
    return;
  }
  if (address < 0x6000 || address > 0x7fff)
    return;

  Byte* work_ram = GetExtendedRAMPointer();
  DCHECK(work_ram);
  const size_t index = GetWorkRAMAddress(address);
  if (work_ram[index] == value)
    return;

  work_ram[index] = value;
  if (index < rom_data()->prg_nvram_size)
    MarkPRGNVRAMDirty();
}

Byte Mapper178::ReadExtendedRAM(Address address) {
  if (address < 0x6000 || address > 0x7fff)
    return static_cast<Byte>(address >> 8);

  Byte* work_ram = GetExtendedRAMPointer();
  DCHECK(work_ram);
  return work_ram[GetWorkRAMAddress(address)];
}

NametableMirroring Mapper178::GetNametableMirroring() {
  return mirroring_;
}

void Mapper178::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(registers_).WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper178::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&registers_).ReadData(&character_ram_);
  UpdateMirroring();
  return Mapper::Deserialize(header, data);
}

size_t Mapper178::GetPRGBank(Address address) const {
  const size_t small_bank = registers_[1] & 0x07;
  const size_t base_bank = static_cast<size_t>(registers_[2]) << 3;
  const size_t selected_bank = base_bank | small_bank;

  if (!(registers_[0] & 0x02)) {
    return selected_bank +
           ((registers_[0] & 0x04) ? 0 : (address >> 14) & 0x01);
  }

  if (address < 0xc000)
    return selected_bank;
  if (registers_[0] & 0x04)
    return base_bank | 0x06 | (registers_[1] & 0x01);
  return base_bank | 0x07;
}

size_t Mapper178::GetWorkRAMAddress(Address address) const {
  const size_t bank = (registers_[3] & 0x03) % work_ram_bank_count_;
  return bank * kWorkRAMBankSize + (address & 0x1fff);
}

void Mapper178::UpdateMirroring() {
  const NametableMirroring mirroring = registers_[0] & 0x01
                                           ? NametableMirroring::kHorizontal
                                           : NametableMirroring::kVertical;
  if (mirroring_ == mirroring)
    return;

  mirroring_ = mirroring;
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
}

}  // namespace nes
}  // namespace kiwi
