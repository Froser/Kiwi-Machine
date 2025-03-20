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

#include "nes/mappers/mapper048.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
constexpr size_t k1KBank = 1024;
constexpr size_t k2KBank = 2 * 1024;
constexpr size_t k8KBank = 8 * 1024;

Mapper048::Mapper048(Cartridge* cartridge) : Mapper(cartridge) {
  mirroring_ = rom_data()->name_table_mirroring;
  chr_1k_bank_count_ = rom_data()->CHR.size() / k1KBank;
  chr_2k_bank_count_ = rom_data()->CHR.size() / k2KBank;
  prg_8k_bank_count_ = rom_data()->PRG.size() / k8KBank;
}

Mapper048::~Mapper048() = default;

void Mapper048::ResetRegisters() {
  memset(prg_regs_, 0, sizeof(prg_regs_));
  memset(chr_regs_, 0, sizeof(chr_regs_));
  irq_counter_ = irq_latch_ = 0;
  irq_enabled_ = false;
}

void Mapper048::Reset() {
  ResetRegisters();
}

void Mapper048::WritePRG(Address address, Byte value) {
  switch (address) {
    case 0x8000: {
      if (type_ == Type::kMapper33) {
        mirroring_ = (value & 0x40) ? NametableMirroring::kHorizontal
                                    : NametableMirroring::kVertical;
        mirroring_changed_callback().Run();
        prg_regs_[0] = value & 0x3f;
      } else {
        DCHECK(type_ == Type::kMapper48);
        prg_regs_[0] = value;
      }
      break;
    }
    case 0x8001:
      prg_regs_[1] = value & 0x3f;
      break;
    case 0x8002:
    case 0x8003:
      chr_regs_[address - 0x8002] = value;
      break;
    case 0xa000:
    case 0xa001:
    case 0xa002:
    case 0xa003:
      chr_regs_[address - 0xa000 + 2] = value;
      break;
    case 0xc000:
      irq_latch_ = value;
      irq_counter_ = irq_latch_;
      break;
    case 0xc001:
      irq_counter_ = irq_latch_;
      break;
    case 0xc002:
      irq_enabled_ = true;
      break;
    case 0xc003:
      irq_enabled_ = false;
      break;
    case 0xe000:
      if (type_ == Type::kMapper48) {
        mirroring_ = (value & 0x40) ? NametableMirroring::kHorizontal
                                    : NametableMirroring::kVertical;
        mirroring_changed_callback().Run();
      }
      break;
    default:
      break;
  }
}

Byte Mapper048::ReadPRG(Address address) {
  Address offset = address & 0x1fff;
  switch ((address >> 13) & 0x3) {
    case 0:  // $8000-$9FFF
      return rom_data()
          ->PRG[k8KBank * (prg_regs_[0] % prg_8k_bank_count_) + offset];
    case 1:  // $A000-$BFFF
      return rom_data()
          ->PRG[k8KBank * (prg_regs_[1] % prg_8k_bank_count_) + offset];
    case 2:  // $C000-$DFFF
      return rom_data()->PRG[k8KBank * (prg_8k_bank_count_ - 2) + offset];
    case 3:  // $E000-$FFFF
      return rom_data()->PRG[k8KBank * (prg_8k_bank_count_ - 1) + offset];
  }
  DCHECK(false) << "Shouldn't be here";
  return 0;
}

void Mapper048::WriteCHR(Address address, Byte value) {}

Byte Mapper048::ReadCHR(Address address) {
  //  $0000   $0400   $0800   $0C00   $1000   $1400   $1800   $1C00
  //  +---------------+---------------+-------+-------+-------+-------+
  //  |     $8002     |     $8003     | $A000 | $A001 | $A002 | $A003 |
  //  +---------------+---------------+-------+-------+-------+-------+
  if (address < 0x800) {
    return rom_data()
        ->CHR[k2KBank * (chr_regs_[0] % chr_2k_bank_count_) + address];
  } else if (address < 0x1000) {
    return rom_data()->CHR[k2KBank * (chr_regs_[1] % chr_2k_bank_count_) +
                           (address - 0x800)];
  } else {
    return rom_data()->CHR[k1KBank * (chr_regs_[((address >> 10) & 0x3) + 2] %
                                      chr_1k_bank_count_) +
                           (address & 0x3ff)];
  }
}

NametableMirroring Mapper048::GetNametableMirroring() {
  return mirroring_;
}

void Mapper048::ScanlineIRQ(int scanline, bool render_enabled) {
  if (type_ == Type::kMapper48) {
    if (scanline >= 0 && scanline < 240 && render_enabled) {
      if (irq_enabled_) {
        if (++irq_counter_ == 0) {
          irq_counter_ = 0;
          irq_enabled_ = false;
          irq_callback().Run();
        }
      }
    }
  }
}

void Mapper048::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(prg_regs_)
      .WriteData(chr_regs_)
      .WriteData(mirroring_)
      .WriteData(irq_counter_)
      .WriteData(irq_latch_)
      .WriteData(irq_enabled_);
  Mapper::Serialize(data);
}

bool Mapper048::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&prg_regs_)
      .ReadData(&chr_regs_)
      .ReadData(&mirroring_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_latch_)
      .ReadData(&irq_enabled_);
  mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
