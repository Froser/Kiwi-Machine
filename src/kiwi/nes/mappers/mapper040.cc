// Copyright (C) 2023 Yisi Yu
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

#include "nes/mappers/mapper040.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

constexpr uint32_t kPRGBankSize = 0x2000;

// See https://www.nesdev.org/40.txt for more details.
// Registers:
// ---------------------------
// Range,Mask:   $8000-FFFF, $E000
//
//   $8000:  Disable and acknowledge IRQ
//   $A000:  Enable IRQ
//   $C000:  Outer bank register (Submapper 1 only)
//   $E000:  8 KiB bank mapped at $C000
//
// PRG Setup:
// ---------------------------
//
//       $6000   $8000   $A000   $C000   $E000
//     +-------+-------+-------+-------+-------+
//     | { 6 } | { 4 } | { 5 } | $E000 | { 7 } |
//     +-------+-------+-------+-------+-------+
Mapper040::Mapper040(Cartridge* cartridge) : Mapper(cartridge) {
  if (cartridge->GetRomData()->CHR.size() == 0) {
    uses_character_ram_ = true;
    character_ram_.resize(0x2000);
  } else {
    uses_character_ram_ = false;
  }
}

Mapper040::~Mapper040() = default;

void Mapper040::WritePRG(Address address, Byte value) {
  if (0x8000 <= address && address <= 0x9fff) {
    // Disable and reset IRQ counter
    irq_enabled_ = false;
    irq_count_ = 0;
  } else if (0xa000 <= address && address <= 0xbfff) {
    // Enable IRQ counter
    irq_enabled_ = true;
  } else if (0xe000 <= address && address <= 0xffff) {
    // Select bank
    select_prg_ = value & 0x7;
  }
}

Byte Mapper040::ReadPRG(Address address) {
  DCHECK(address >= 0x6000);
  // SMB2J will read PRG with an address less than 0x6000.
  if (address < 0x6000)
    return 0;

  if (address < 0x8000)
    return cartridge()
        ->GetRomData()
        ->PRG[(kPRGBankSize * 6) | (address & 0x1fff)];

  if (address < 0xa000)
    return cartridge()
        ->GetRomData()
        ->PRG[(kPRGBankSize * 4) | (address & 0x1fff)];

  if (address < 0xc000)
    return cartridge()
        ->GetRomData()
        ->PRG[(kPRGBankSize * 5) | (address & 0x1fff)];

  if (address < 0xe000) {
    // Selectable:
    return cartridge()
        ->GetRomData()
        ->PRG[(kPRGBankSize * select_prg_) | (address & 0x1fff)];
  }

  return cartridge()
      ->GetRomData()
      ->PRG[(kPRGBankSize * 7) | (address & 0x1fff)];
}

void Mapper040::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address] = value;
}

Byte Mapper040::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address];

  return cartridge()->GetRomData()->CHR[address];
}

Byte Mapper040::ReadExtendedRAM(Address address) {
  return ReadPRG(address);
}

void Mapper040::M2CycleIRQ() {
  if (irq_enabled_) {
    if (irq_count_ < 4096) {
      ++irq_count_;
    } else {
      irq_enabled_ = false;
      irq_callback().Run();
    }
  }
}

void Mapper040::Serialize(EmulatorStates::SerializableStateData& data) {
  if (uses_character_ram_)
    data.WriteData(character_ram_);

  data.WriteData(select_prg_).WriteData(irq_enabled_).WriteData(irq_count_);
  Mapper::Serialize(data);
}

bool Mapper040::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (uses_character_ram_)
    data.ReadData(&character_ram_);

  data.ReadData(&select_prg_).ReadData(&irq_enabled_).ReadData(&irq_count_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
