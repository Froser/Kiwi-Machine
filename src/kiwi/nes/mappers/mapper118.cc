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

#include "nes/mappers/mapper118.h"

#include "base/check.h"

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kNametableSize = 0x0400;

}  // namespace

Mapper118::Mapper118(Cartridge* cartridge) : Mapper004(cartridge) {}

Mapper118::~Mapper118() = default;

void Mapper118::WritePRG(Address address, Byte value) {
  const Address decoded_address = address & 0xe001;
  if (decoded_address == 0xa000)
    return;

  if (decoded_address == 0x8001)
    UpdateNametablePages(value);

  Mapper004::WritePRG(address, value);
}

bool Mapper118::UsesCustomPPUMemoryMapping() const {
  return true;
}

Byte Mapper118::ReadPPUMemoryByte(Byte* ciram, Address address) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000)
    return Mapper004::ReadCHR(address);

  const Byte page = nametable_pages_[(address >> 10) & 0x03];
  return ciram[page * kNametableSize + (address & 0x03ff)];
}

void Mapper118::WritePPUMemoryByte(Byte* ciram, Address address, Byte value) {
  DCHECK(ciram);
  DCHECK_LT(address, 0x3000);
  if (address < 0x2000) {
    Mapper004::WriteCHR(address, value);
    return;
  }

  const Byte page = nametable_pages_[(address >> 10) & 0x03];
  ciram[page * kNametableSize + (address & 0x03ff)] = value;
}

void Mapper118::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(nametable_pages_);
  Mapper004::Serialize(data);
}

bool Mapper118::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&nametable_pages_);
  return Mapper004::Deserialize(header, data);
}

void Mapper118::UpdateNametablePages(Byte value) {
  const Byte page = value >> 7;
  if (!chr_mode_) {
    if (target_register_ == 0) {
      nametable_pages_[0] = page;
      nametable_pages_[1] = page;
    } else if (target_register_ == 1) {
      nametable_pages_[2] = page;
      nametable_pages_[3] = page;
    }
    return;
  }

  if (target_register_ >= 2 && target_register_ <= 5)
    nametable_pages_[target_register_ - 2] = page;
}

}  // namespace nes
}  // namespace kiwi
