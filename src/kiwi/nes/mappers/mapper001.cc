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
}

Mapper001::~Mapper001() = default;

void Mapper001::WritePRG(Address address, Byte value) {
  // Load Register: $8000-$FFFF
  if (address < 0x8000)
    return;

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
      // Only on the fifth write does the address matter, and even then, only
      // bits 14 and 13 of the address matter because the mapper doesn't see the
      // lower address bits (similar to the mirroring seen with PPU registers).
      // After the fifth write, the shift register is cleared automatically, so
      // writing again with bit 7 set to clear the shift register is not needed.
      WriteRegister(address, shift_register_);
      shift_register_ = 0;
      write_count_ = 0;
    }
  } else {
    shift_register_ = 0;
    write_count_ = 0;
    prg_mode_ = 3;
    mirroring_ = NametableMirroring::kOneScreenLower;
  }
}

Byte Mapper001::ReadPRG(Address address) {
  constexpr uint32_t kPRGBankSize = 0x4000;
  DCHECK(address >= 0x8000);
  if (address < 0xc000) {
    // $8000-$BFFF
    int bank = 0;
    switch (prg_mode_) {
      case 0:
      case 1:
        bank = prg_reg_ & 0xfe;
        break;
      case 2:
        bank = 0;
        break;
      case 3:
        bank = prg_reg_;
        break;
      default:
        CHECK(false) << "Shouldn't happen.";
        return 0;
    }
    uint32_t index = (kPRGBankSize * bank) | (address & 0x3fff);
    return cartridge()->GetRomData()->PRG[index];
  } else {
    // $C000-$FFFF
    int bank = 0;
    switch (prg_mode_) {
      case 0:
      case 1:
        bank = (prg_reg_ & 0xfe) | 1; // Added 16 kb
        break;
      case 2:
        bank = prg_reg_;
        break;
      case 3:
        bank = cartridge()->GetRomData()->PRG.size() / kPRGBankSize - 1;
        break;
      default:
        CHECK(false) << "Shouldn't happen.";
        return 0;
    }
    uint32_t index = (kPRGBankSize * bank) | (address & 0x3fff);
    return cartridge()->GetRomData()->PRG[index];
  }
}

void Mapper001::WriteCHR(Address address, Byte value) {
  if (uses_character_ram_)
    character_ram_[address] = value;
}

Byte Mapper001::ReadCHR(Address address) {
  if (uses_character_ram_)
    return character_ram_[address];

  constexpr uint32_t kCHRBankSize = 0x1000;
  if (address < 0x1000) {
    uint32_t index = (kCHRBankSize * chr_reg_0_) | (address & 0x3fff);
    return cartridge()->GetRomData()->CHR[index];
  } else if (address <= 0x2000) {
    uint32_t bank = (chr_mode_ == 0) ? (chr_reg_0_ + 1) : (chr_reg_1_);
    uint32_t index = (kCHRBankSize * bank) | ((address - 0x1000) & 0x3fff);
    return cartridge()->GetRomData()->CHR[index];
  }

  CHECK(false) << "Shouldn't happen.";
  return 0;
}

NametableMirroring Mapper001::GetNametableMirroring() {
  return mirroring_;
}

void Mapper001::WriteRegister(Address address, Byte value) {
  if (address <= 0x9fff) {
    switch (value & 0x3) {
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
        LOG(ERROR) << "Wrong mirroring: " << (value & 0x3);
        break;
    }

    DCHECK(mirroring_changed_callback());
    mirroring_changed_callback().Run();

    chr_mode_ = (value & 0x10) >> 4;
    prg_mode_ = (value & 0xc) >> 2;
  } else if (address <= 0xbfff) {  // $A000-$BFFF
    chr_reg_0_ = value & 0x1f;
  } else if (address <= 0xdfff) {  // $C000-$DFFF
    chr_reg_1_ = value & 0x1f;
  } else {  // $E000-$FFFF
    if ((value & 0x10) == 0x10) {
      LOG(INFO) << "PRG-RAM activated";
    }

    prg_reg_ = value & 0xf;
  }
}

void Mapper001::Serialize(EmulatorStates::SerializableStateData& data) {
  if (uses_character_ram_)
    data.WriteData(character_ram_);

  data.WriteData(shift_register_)
      .WriteData(write_count_)
      .WriteData(chr_mode_)
      .WriteData(prg_mode_)
      .WriteData(chr_reg_0_)
      .WriteData(chr_reg_1_)
      .WriteData(prg_reg_)
      .WriteData(mirroring_);

  Mapper::Serialize(data);
}

bool Mapper001::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (uses_character_ram_)
    data.ReadData(&character_ram_);

  data.ReadData(&shift_register_)
      .ReadData(&write_count_)
      .ReadData(&chr_mode_)
      .ReadData(&prg_mode_)
      .ReadData(&chr_reg_0_)
      .ReadData(&chr_reg_1_)
      .ReadData(&prg_reg_)
      .ReadData(&mirroring_);

  mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
