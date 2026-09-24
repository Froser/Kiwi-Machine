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

#include "nes/mappers/mapper177.h"

#include "base/check.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kPRGBankSize = 0x8000;
constexpr size_t kWorkRAMBankSize = 0x2000;
constexpr size_t kCHRBankSize = 0x1000;
constexpr size_t kDefaultCHRRAMSize = 0x2000;
constexpr size_t kExtendedCHRRAMSize = 0x4000;
constexpr size_t kNametableSize = 0x0400;

}  // namespace

Mapper177::Mapper177(Cartridge* cartridge)
    : Mapper(cartridge),
      uses_16k_chr_ram_(rom_data()->submapper == 1),
      mirroring_(rom_data()->name_table_mirroring) {
  DCHECK_EQ(rom_data()->PRG.size() % kPRGBankSize, 0u);
  prg_bank_count_ = rom_data()->PRG.size() / kPRGBankSize;
  DCHECK_GT(prg_bank_count_, 0u);

  EnsurePRGRAM(kWorkRAMBankSize);
  const size_t work_ram_size =
      rom_data()->prg_ram_size + rom_data()->prg_nvram_size;
  DCHECK_EQ(work_ram_size % kWorkRAMBankSize, 0u);
  work_ram_bank_count_ = work_ram_size / kWorkRAMBankSize;
  DCHECK_GT(work_ram_bank_count_, 0u);

  DCHECK(rom_data()->CHR.empty());
  character_ram_.resize(uses_16k_chr_ram_ ? kExtendedCHRRAMSize
                                          : kDefaultCHRRAMSize);
}

Mapper177::~Mapper177() = default;

void Mapper177::Reset() {
  control_ = 0;
  chr_bank_ = 0;
  const NametableMirroring mirroring = rom_data()->name_table_mirroring;
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

void Mapper177::WritePRG(Address address, Byte value) {
  if (address < 0x8000)
    return;

  control_ = value;
  if (uses_16k_chr_ram_)
    return;

  const NametableMirroring mirroring = value & 0x20
                                           ? NametableMirroring::kHorizontal
                                           : NametableMirroring::kVertical;
  if (mirroring_ != mirroring) {
    mirroring_ = mirroring;
    if (mirroring_changed_callback())
      mirroring_changed_callback().Run();
  }
}

Byte Mapper177::ReadPRG(Address address) {
  DCHECK_GE(address, 0x8000);
  const size_t bank = (control_ & 0x1f) % prg_bank_count_;
  return rom_data()->PRG[bank * kPRGBankSize + (address & 0x7fff)];
}

void Mapper177::WriteCHR(Address address, Byte value) {
  character_ram_[GetAbsoluteCHRAddress(address)] = value;
}

Byte Mapper177::ReadCHR(Address address) {
  return character_ram_[GetAbsoluteCHRAddress(address)];
}

uint32_t Mapper177::GetAbsoluteCHRAddress(Address address) {
  address &= 0x1fff;
  if (!uses_16k_chr_ram_ || !(control_ & 0x20))
    return address;

  const size_t bank = address < 0x1000 ? chr_bank_ : 3;
  return static_cast<uint32_t>(bank * kCHRBankSize + (address & 0x0fff));
}

void Mapper177::WriteExtendedRAM(Address address, Byte value) {
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

Byte Mapper177::ReadExtendedRAM(Address address) {
  if (address < 0x6000 || address > 0x7fff)
    return static_cast<Byte>(address >> 8);

  Byte* work_ram = GetExtendedRAMPointer();
  DCHECK(work_ram);
  return work_ram[GetWorkRAMAddress(address)];
}

NametableMirroring Mapper177::GetNametableMirroring() {
  return mirroring_;
}

bool Mapper177::UsesCustomPPUMemoryMapping() const {
  return uses_16k_chr_ram_;
}

Byte Mapper177::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return ReadCHR(address);

  LatchCHRBank(address);
  return ciram[GetCIRAMAddress(address)];
}

void Mapper177::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    WriteCHR(address, value);
    return;
  }

  LatchCHRBank(address);
  ciram[GetCIRAMAddress(address)] = value;
}

void Mapper177::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(control_)
      .WriteData(chr_bank_)
      .WriteData(mirroring_)
      .WriteData(character_ram_);
  Mapper::Serialize(data);
}

bool Mapper177::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&control_)
      .ReadData(&chr_bank_)
      .ReadData(&mirroring_)
      .ReadData(&character_ram_);
  if (mirroring_changed_callback())
    mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

size_t Mapper177::GetWorkRAMAddress(Address address) const {
  const size_t bank = ((control_ >> 6) & 0x03) % work_ram_bank_count_;
  return bank * kWorkRAMBankSize + (address & 0x1fff);
}

size_t Mapper177::GetCIRAMAddress(Address address) const {
  const size_t nametable = (address >> 10) & 0x03;
  size_t page = 0;
  switch (mirroring_) {
    case NametableMirroring::kHorizontal:
      page = nametable >> 1;
      break;
    case NametableMirroring::kVertical:
      page = nametable & 0x01;
      break;
    case NametableMirroring::kOneScreenLower:
      page = 0;
      break;
    case NametableMirroring::kOneScreenHigher:
      page = 1;
      break;
    case NametableMirroring::kFourScreen:
      DCHECK(false);
      break;
  }
  return page * kNametableSize + (address & 0x03ff);
}

void Mapper177::LatchCHRBank(Address address) {
  if (control_ & 0x20)
    chr_bank_ = static_cast<Byte>((address >> 8) & 0x03);
}

}  // namespace nes
}  // namespace kiwi
