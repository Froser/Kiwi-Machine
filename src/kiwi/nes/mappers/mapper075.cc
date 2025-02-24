// Copyright (C) 2025 Yisi Yu
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

#include "nes/mappers/mapper075.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

constexpr size_t kPRGSize = 16 * 0x2000;  // 128 KB
constexpr size_t kPRGBankSize = 0x2000;   // 8 KB
constexpr size_t kPRGBankCount = kPRGSize / kPRGBankSize;

constexpr size_t kCHRSize = 16 * 0x2000;  // 128 KB
constexpr size_t kCHRBankSize = 0x1000;   // 4 KB
constexpr size_t kCHRBankCount = kCHRSize / kCHRBankSize;

Mapper075::Mapper075(Cartridge* cartridge) : Mapper(cartridge) {
  if (rom_data()->PRG.size() != kPRGSize) {
    LOG(ERROR) << "PRG size mismatch. 128 KB is expected, while this "
                  "cartridge's PRG size is "
               << rom_data()->PRG.size();
  }

  if (rom_data()->CHR.size() != kCHRSize) {
    LOG(ERROR) << "CHR size mismatch. 128 KB is expected, while this "
                  "cartridge's CHR size is "
               << rom_data()->CHR.size();
  }

  mirroring_ = rom_data()->name_table_mirroring;
}

Mapper075::~Mapper075() = default;

void Mapper075::WritePRG(Address address, Byte value) {
  switch (address & 0xf000) {
    case 0x8000:
      prg_regs[0] = value & 0xf;
      break;
    case 0x9000:
      // 7  bit  0
      // ---------
      // .... .BAM
      //       |||
      //       ||+- Mirroring  (0: Vertical; 1: Horizontal)
      //       |+-- High Bit of 4 KB CHR bank at PPU $0000
      //       +--- High Bit of 4 KB CHR bank at PPU $1000
      mirroring_ = ((value & 1) == 0) ? NametableMirroring::kVertical
                                      : NametableMirroring::kHorizontal;
      chr_regs[0] = (chr_regs[0] & 0x0F) | ((value & 0x02) << 3);
      chr_regs[1] = (chr_regs[1] & 0x0F) | ((value & 0x04) << 2);
      mirroring_changed_callback().Run();
      break;
    case 0xa000:
      prg_regs[1] = value & 0xf;
      break;
    case 0xc000:
      prg_regs[2] = value & 0xf;
      break;
    case 0xe000:
      // 7  bit  0
      // ---------
      // .... CCCC
      //      ||||
      //      ++++- Low 4 bits of 4 KB CHR bank at PPU $0000
      chr_regs[0] = (chr_regs[0] & 0x10) | (value & 0x0F);
      break;
    case 0xf000:
      // 7  bit  0
      // ---------
      // .... CCCC
      //      ||||
      //      ++++- Low 4 bits of 4 KB CHR bank at PPU $1000
      chr_regs[1] = (chr_regs[1] & 0x10) | (value & 0x0F);
      break;
  }
}

Byte Mapper075::ReadPRG(Address address) {
  DCHECK(address >= 0x8000);
  if (address < 0xa000)
    return rom_data()->PRG[(prg_regs[0] % kPRGBankCount) * kPRGBankSize +
                           (address - 0x8000)];

  if (address < 0xc000)
    return rom_data()->PRG[(prg_regs[1] % kPRGBankCount) * kPRGBankSize +
                           (address - 0xa000)];

  if (address < 0xe000)
    return rom_data()->PRG[(prg_regs[2] % kPRGBankCount) * kPRGBankSize +
                           (address - 0xc000)];

  return rom_data()
      ->PRG[(kPRGBankCount - 1) * kPRGBankSize + (address - 0xe000)];
}

void Mapper075::WriteCHR(Address address, Byte value) {}

Byte Mapper075::ReadCHR(Address address) {
  if (address < 0x1000)
    return rom_data()
        ->CHR[(chr_regs[0] % kCHRBankCount) * kCHRBankSize + address];

  DCHECK(address < 0x2000);
  return rom_data()
      ->CHR[(chr_regs[1] % kCHRBankCount) * kCHRBankSize + (address - 0x1000)];
}

void Mapper075::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_regs[0])
      .WriteData(prg_regs[1])
      .WriteData(prg_regs[2])
      .WriteData(chr_regs[0])
      .WriteData(chr_regs[1])
      .WriteData(mirroring_);
  Mapper::Serialize(data);
}

bool Mapper075::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_regs[0])
      .ReadData(&prg_regs[1])
      .ReadData(&prg_regs[2])
      .ReadData(&chr_regs[0])
      .ReadData(&chr_regs[1])
      .ReadData(&mirroring_);
  return Mapper::Deserialize(header, data);
}

NametableMirroring Mapper075::GetNametableMirroring() {
  return mirroring_;
}

}  // namespace nes
}  // namespace kiwi
