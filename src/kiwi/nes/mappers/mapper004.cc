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

#include "nes/mappers/mapper004.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
constexpr int kPRGBankSize = 8192;
constexpr int kCHRBankSize = 1024;

Mapper004::Mapper004(Cartridge* cartridge) : Mapper(cartridge) {
  if (cartridge->GetRomData()->CHR.size() == 0) {
    uses_character_ram_ = true;
    character_ram_.resize(0x2000);
  } else {
    uses_character_ram_ = false;
  }

  mirroring_ram_.resize(4 * 1024);

  prg_banks_count_ = cartridge->GetRomData()->PRG.size() / kPRGBankSize;
}

Mapper004::~Mapper004() = default;

void Mapper004::WritePRG(Address address, Byte value) {
  bool is_even = !(address & 1);
  if (address >= 0x8000 && address <= 0x9fff) {
    if (is_even) {
      // Select bank
      target_register_ = value & 0x7;
      prg_mode_ = value & 0x40;
      chr_mode_ = value & 0x80;
    } else {
      bank_register_[target_register_] = value;
    }
  } else if (address >= 0xa000 && address <= 0xbfff) {
    if (is_even) {
      // Mirroring
      if (cartridge()->GetRomData()->name_table_mirroring ==
          NametableMirroring::kFourScreen) {
        mirroring_ = NametableMirroring::kFourScreen;
      } else if (value & 0x01) {
        mirroring_ = NametableMirroring::kHorizontal;
      } else {
        mirroring_ = NametableMirroring::kVertical;
      }
      mirroring_changed_callback().Run();
    } else {
      // PRG-RAM protect
      // https://www.nesdev.org/wiki/MMC3#PRG_RAM_protect_($A001-$BFFF,_odd)
      // Though these bits are functional on the MMC3, their main
      // purpose is to write-protect save RAM during power-off.
      // Many emulators choose not to implement them as part of
      // iNES Mapper 4 to avoid an incompatibility with the MMC6.
    }
  } else if (address >= 0xc000 && address <= 0xdfff) {
    if (is_even) {
      irq_latch_ = value;
    } else {
      irq_counter_ = 0;
      irq_reload_ = true;
    }
  } else if (address >= 0xe000) {
    // IRQ enabled if odd.
    if (is_even) {
      irq_enabled_ = false;
      irq_flag_ = false;
    } else {
      irq_enabled_ = true;
    }
  }
}

Byte Mapper004::ReadPRG(Address address) {
  if (0x8000 <= address && address <= 0x9fff) {
    int bank = prg_mode_ ? prg_banks_count_ - 2 : bank_register_[6];
    Address offset = address & 0x1fff;
    int index = ((kPRGBankSize * bank) | offset) %
                cartridge()->GetRomData()->PRG.size();
    return cartridge()->GetRomData()->PRG[index];
  }

  if (0xa000 <= address && address <= 0xbfff) {
    int bank = bank_register_[7];
    Address offset = address & 0x1fff;
    int index = ((kPRGBankSize * bank) | offset) %
                cartridge()->GetRomData()->PRG.size();
    return cartridge()->GetRomData()->PRG[index];
  }

  if (0xc000 <= address && address <= 0xdfff) {
    int bank = prg_mode_ ? bank_register_[6] : prg_banks_count_ - 2;
    Address offset = address & 0x1fff;
    int index = ((kPRGBankSize * bank) | offset) %
                cartridge()->GetRomData()->PRG.size();
    return cartridge()->GetRomData()->PRG[index];
  }

  if (0xe000 <= address && address <= 0xffff) {
    int bank = prg_banks_count_ - 1;
    Address offset = address & 0x1fff;
    int index = ((kPRGBankSize * bank) | offset) %
                cartridge()->GetRomData()->PRG.size();
    return cartridge()->GetRomData()->PRG[index];
  }

  DCHECK(false);
  return 0;
}

void Mapper004::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_) {
    character_ram_[address] = value;
    return;
  }

  if (address >= 0x2000 && address <= 0x2fff)
    mirroring_ram_[address - 0x2000] = value;
}

Byte Mapper004::ReadCHR(Address address) {
  if (uses_character_ram_) {
    return character_ram_[address];
  }

  int bank = 0;
  if (address <= 0x1fff) {
    switch (address & 0xfc00) {
      case 0x0000:
        bank = (!chr_mode_) ? (bank_register_[0] & 0xfe) : bank_register_[2];
        break;
      case 0x0400:
        bank = (!chr_mode_) ? (bank_register_[0] | 0x01) : bank_register_[3];
        break;
      case 0x0800:
        bank = (!chr_mode_) ? (bank_register_[1] & 0xfe) : bank_register_[4];
        break;
      case 0x0c00:
        bank = (!chr_mode_) ? (bank_register_[1] | 0x01) : bank_register_[5];
        break;
      case 0x1000:
        bank = (!chr_mode_) ? (bank_register_[2]) : (bank_register_[0] & 0xfe);
        break;
      case 0x1400:
        bank = (!chr_mode_) ? (bank_register_[3]) : (bank_register_[0] | 0x01);
        break;
      case 0x1800:
        bank = (!chr_mode_) ? (bank_register_[4]) : (bank_register_[1] & 0xfe);
        break;
      case 0x1c00:
        bank = (!chr_mode_) ? (bank_register_[5]) : (bank_register_[1] | 0x01);
        break;
      default:
        DCHECK(false) << "Shouldn't happen.";
    }

    Address offset = address % 0x0400;
    int index = ((kCHRBankSize * bank) | offset) %
                cartridge()->GetRomData()->CHR.size();
    return cartridge()->GetRomData()->CHR[index];
  } else if (address <= 0x2fff) {
    return mirroring_ram_[address - 0x2000];
  }

  DCHECK(false);
  return 0;
}

NametableMirroring Mapper004::GetNametableMirroring() {
  return mirroring_;
}

void Mapper004::PPUAddressChanged(Address address) {
  Address prev_a12 = (last_vram_address_ >> 12) & 1;
  Address cur_a12 = (address >> 12) & 1;

  if (prev_a12 == 0 && cur_a12 == 1) {
    StepIRQCounter();
  }

  last_vram_address_ = address;
}

void Mapper004::ScanlineIRQ() {
  if (irq_flag_) {
    irq_callback().Run();
  } else {
    StepIRQCounter();
  }
}

void Mapper004::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(last_vram_address_)
      .WriteData(target_register_)
      .WriteData(prg_mode_)
      .WriteData(chr_mode_)
      .WriteData(bank_register_[0])
      .WriteData(bank_register_[1])
      .WriteData(bank_register_[2])
      .WriteData(bank_register_[3])
      .WriteData(bank_register_[4])
      .WriteData(bank_register_[5])
      .WriteData(bank_register_[6])
      .WriteData(bank_register_[7])
      .WriteData(mirroring_)
      .WriteData(irq_enabled_)
      .WriteData(irq_counter_)
      .WriteData(irq_latch_)
      .WriteData(irq_reload_)
      .WriteData(irq_flag_)
      .WriteData(mirroring_ram_);

  if (uses_character_ram_)
    data.WriteData(uses_character_ram_).WriteData(character_ram_);

  Mapper::Serialize(data);
}

bool Mapper004::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&last_vram_address_)
      .ReadData(&target_register_)
      .ReadData(&prg_mode_)
      .ReadData(&chr_mode_)
      .ReadData(&bank_register_[0])
      .ReadData(&bank_register_[1])
      .ReadData(&bank_register_[2])
      .ReadData(&bank_register_[3])
      .ReadData(&bank_register_[4])
      .ReadData(&bank_register_[5])
      .ReadData(&bank_register_[6])
      .ReadData(&bank_register_[7])
      .ReadData(&mirroring_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_counter_)
      .ReadData(&irq_latch_)
      .ReadData(&irq_reload_)
      .ReadData(&irq_flag_)
      .ReadData(&mirroring_ram_);

  if (uses_character_ram_)
    data.ReadData(&uses_character_ram_).ReadData(&character_ram_);
  return Mapper::Deserialize(header, data);
}

void Mapper004::StepIRQCounter() {
  if (irq_counter_ == 0 || irq_reload_) {
    irq_counter_ = irq_latch_;
  } else {
    irq_counter_ -= 1;
  }

  if (irq_counter_ == 0 && irq_enabled_) {
    irq_flag_ = true;
  }

  irq_reload_ = false;
}

}  // namespace nes
}  // namespace kiwi
