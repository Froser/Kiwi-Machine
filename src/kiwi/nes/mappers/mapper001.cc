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

#include "nes/mappers/mapper001.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
Mapper001::Mapper001(Cartridge* cartridge) : Mapper(cartridge) {
  if (cartridge->GetRomData()->CHR.size() == 0) {
    uses_character_ram_ = true;
    character_ram_.resize(0x2000);
  } else {
    uses_character_ram_ = false;
  }

  prg_bank_0_offset_ = 0;
  prg_bank_1_offset_ = (cartridge->GetRomData()->PRG.size() - 0x4000);
}

Mapper001::~Mapper001() = default;

void Mapper001::WritePRG(Address address, Byte value) {
  // https://www.nesdev.org/wiki/MMC1
  // Writing a value with bit 7 set ($80 through $FF) to any address in
  // $8000-$FFFF clears the shift register to its initial state. To change a
  // register's value, the CPU writes five times with bit 7 clear and one bit of
  // the desired value in bit 0 (starting with the low bit of the value).
  if (!(value & 0x80)) {
    // On the first four writes, the MMC1 shifts bit 0 into a shift register. On
    // the fifth write, the MMC1 copies bit 0 and the shift register contents
    // into an internal register selected by bits 14 and 13 of the address, and
    // then it clears the shift register.
    shift_register_ = (shift_register_ >> 1) | ((value & 1) << 4);
    ++write_count_;

    if (write_count_ == 5) {
      if (address <= 0x9fff) {  // $8000-$9FFF
        switch (shift_register_ & 0x3) {
          case 0:
            mirroring_ = NametableMirroring::kOneScreenLower;
            break;
          case 1:
            mirroring_ = NametableMirroring::kOneScreenHigher;
            break;
          case 2:
            mirroring_ = NametableMirroring::kVertical;
            break;
          case 3:
            mirroring_ = NametableMirroring::kHorizontal;
            break;
          default:
            LOG(ERROR) << "Wrong mirroring: " << (shift_register_ & 0x3);
            break;
        }

        DCHECK(mirroring_changed_callback());
        mirroring_changed_callback().Run();

        mode_chr_ = (shift_register_ & 0x10) >> 4;
        prg_mode_ = (shift_register_ & 0xc) >> 2;
        CalculatePRGBanksOffset();

        if (mode_chr_ == 0) {
          chr_bank_0_offset_ = 0x1000 * (chr_reg_0_ | 1);
          chr_bank_1_offset_ = chr_bank_0_offset_ + 0x1000;
        } else {
          chr_bank_0_offset_ = (0x1000 * chr_reg_0_);
          chr_bank_1_offset_ = (0x1000 * chr_reg_1_);
        }
      } else if (address <= 0xbfff) {  // $A000-$BFFF
        chr_reg_0_ = shift_register_;
        chr_bank_0_offset_ = (0x1000 * (shift_register_ | (1 - mode_chr_)));
        if (mode_chr_ == 0)
          chr_bank_1_offset_ = chr_bank_0_offset_ + 0x1000;
      } else if (address <= 0xdfff) {  // $C000-$DFFF
        chr_reg_1_ = shift_register_;
        if (mode_chr_ == 1)
          chr_bank_1_offset_ = (0x1000 * shift_register_);
      } else {  // $E000-$FFFF
        if ((shift_register_ & 0x10) == 0x10) {
          LOG(INFO) << "PRG-RAM activated";
        }

        shift_register_ &= 0xf;
        prg_reg_ = shift_register_;
        CalculatePRGBanksOffset();
      }

      shift_register_ = 0;
      write_count_ = 0;
    }
  } else {
    shift_register_ = 0;
    write_count_ = 0;
    prg_mode_ = 3;
    CalculatePRGBanksOffset();
  }
}

Byte Mapper001::ReadPRG(Address address) {
  if (address < 0xc000)
    return *(prg_bank_0() + (address & 0x3fff));

  return *(prg_bank_1() + (address & 0x3fff));
}

void Mapper001::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address] = value;
}

Byte Mapper001::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address];
  else if (address < 0x1000)
    return *(chr_bank_0() + address);
  else
    return *(chr_bank_1() + (address & 0xfff));
}

NametableMirroring Mapper001::GetNametableMirroring() {
  return mirroring_;
}

void Mapper001::CalculatePRGBanksOffset() {
  switch (prg_mode_) {
    case 0:
    case 1:
      prg_bank_0_offset_ = 0x8000 * (prg_reg_ >> 1);
      prg_bank_1_offset_ = prg_bank_0_offset_ + 0x4000;
      break;
    case 2:
      prg_bank_0_offset_ = 0;
      prg_bank_1_offset_ = prg_bank_0_offset_ + 0x4000 * prg_reg_;
      break;
    case 3:
      prg_bank_0_offset_ = (0x4000 * prg_reg_);
      prg_bank_1_offset_ = (cartridge()->GetRomData()->PRG.size() - 0x4000);
      break;
    default:
      LOG(ERROR) << "Wrong program mode: " << prg_mode_;
      return;
  }
}

Byte* Mapper001::chr_bank_0() {
  return cartridge()->GetRomData()->CHR.data() + chr_bank_0_offset_;
}

Byte* Mapper001::chr_bank_1() {
  return cartridge()->GetRomData()->CHR.data() + chr_bank_1_offset_;
}

Byte* Mapper001::prg_bank_0() {
  return cartridge()->GetRomData()->PRG.data() + prg_bank_0_offset_;
}

Byte* Mapper001::prg_bank_1() {
  return cartridge()->GetRomData()->PRG.data() + prg_bank_1_offset_;
}

void Mapper001::Serialize(EmulatorStates::SerializableStateData& data) {
  if (uses_character_ram_)
    data.WriteData(character_ram_);

  data.WriteData(shift_register_)
      .WriteData(write_count_)
      .WriteData(mode_chr_)
      .WriteData(prg_mode_)
      .WriteData(chr_reg_0_)
      .WriteData(chr_reg_1_)
      .WriteData(prg_reg_)
      .WriteData(mirroring_)
      .WriteData(chr_bank_0_offset_)
      .WriteData(chr_bank_1_offset_)
      .WriteData(prg_bank_0_offset_)
      .WriteData(prg_bank_1_offset_);

  Mapper::Serialize(data);
}

bool Mapper001::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (uses_character_ram_)
    data.ReadData(&character_ram_);

  data.ReadData(&shift_register_)
      .ReadData(&write_count_)
      .ReadData(&mode_chr_)
      .ReadData(&prg_mode_)
      .ReadData(&chr_reg_0_)
      .ReadData(&chr_reg_1_)
      .ReadData(&prg_reg_)
      .ReadData(&mirroring_)
      .ReadData(&chr_bank_0_offset_)
      .ReadData(&chr_bank_1_offset_)
      .ReadData(&prg_bank_0_offset_)
      .ReadData(&prg_bank_1_offset_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
