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

#include "nes/mappers/mapper010.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

#define kPRGSize (rom_data()->PRG.size())
constexpr size_t kPRGBankSize = 16 * 1024;
#define kPRGBankCount (kPRGSize / kPRGBankSize)

#define kCHRSize (rom_data()->CHR.size())
constexpr size_t kCHRBankSize = 4 * 1024;
#define kCHRBankCount (kCHRSize / kCHRBankSize)

Mapper010::Mapper010(Cartridge* cartridge) : Mapper(cartridge) {
  mirroring_ = rom_data()->name_table_mirroring;
}

Mapper010::~Mapper010() = default;

void Mapper010::WritePRG(Address address, Byte value) {
  switch (address & 0xf000) {
    case 0xa000:
      select_prg_ = value % kPRGBankCount;
      break;
    case 0xb000:
      chr_regs_[0] = value % kCHRBankCount;
      if (latch_0_ == 0xfd)
        select_chr_first_ = chr_regs_[0];
      break;
    case 0xc000:
      chr_regs_[1] = value % kCHRBankCount;
      if (latch_0_ == 0xfe)
        select_chr_first_ = chr_regs_[1];
      break;
    case 0xd000:
      chr_regs_[2] = value % kCHRBankCount;
      if (latch_1_ == 0xfd)
        select_chr_second_ = chr_regs_[2];
      break;
    case 0xe000:
      chr_regs_[3] = value % kCHRBankCount;
      if (latch_1_ == 0xfe)
        select_chr_second_ = chr_regs_[3];
      break;
    case 0xf000:
      mirroring_ = (value & 0x1) == 0 ? NametableMirroring::kVertical
                                      : NametableMirroring::kHorizontal;
      mirroring_changed_callback().Run();
      break;
  }
}

Byte Mapper010::ReadPRG(Address address) {
  // CPU $8000-$BFFF: 16 KB switchable PRG ROM bank
  if (address < 0xc000) {
    size_t banks = select_prg_ % kPRGBankCount;
    return rom_data()->PRG[kPRGBankSize * banks + (address - 0x8000)];
  }

  // CPU $C000-$FFFF: 16 KB PRG ROM bank, fixed to the last bank
  return rom_data()
      ->PRG[kPRGBankSize * (kPRGBankCount - 1) + (address - 0xc000)];
}

void Mapper010::WriteCHR(Address address, Byte value) {}

Byte Mapper010::ReadCHR(Address address) {
  if ((address & 0x1ff0) == 0x0fd0 && latch_0_ != 0xfd) {
    latch_0_ = 0xfd;
    select_chr_first_ = chr_regs_[0];
  } else if ((address & 0x1ff0) == 0x0fe0 && latch_0_ != 0xfe) {
    latch_0_ = 0xfe;
    select_chr_first_ = chr_regs_[1];
  } else if ((address & 0x1ff0) == 0x1fd0 && latch_1_ != 0xfd) {
    latch_1_ = 0xfd;
    delay_change_chr_bank(2);
  } else if ((address & 0x1ff0) == 0x1fe0 && latch_1_ != 0xfe) {
    latch_1_ = 0xfe;
    delay_change_chr_bank(3);
  }

  Byte ret = 0;
  switch (address & 0xf000) {
    case 0x0000:
      ret = rom_data()
                ->CHR[select_chr_first_ * kCHRBankSize + (address & 0x0fff)];
      break;
    case 0x1000:
      ret = rom_data()
                ->CHR[select_chr_second_ * kCHRBankSize + (address & 0x0fff)];
      break;
    default:
      CHECK(false) << "Shouldn't be here";
      break;
  }

  // Do real CHR bank switch when the right tile is read.
  if (bg_chr_change_countdown_ > 0) {
    --bg_chr_change_countdown_;
    if (bg_chr_change_countdown_ == 0)
      select_chr_second_ = chr_regs_[delayed_chr_bank_index_];
  }

  return ret;
}

NametableMirroring Mapper010::GetNametableMirroring() {
  return mirroring_;
}

void Mapper010::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(latch_0_)
      .WriteData(latch_1_)
      .WriteData(select_chr_first_)
      .WriteData(select_chr_second_)
      .WriteData(chr_regs_[0])
      .WriteData(chr_regs_[1])
      .WriteData(chr_regs_[2])
      .WriteData(chr_regs_[3])
      .WriteData(mirroring_)
      .WriteData(select_prg_)
      .WriteData(bg_chr_change_countdown_)
      .WriteData(delayed_chr_bank_index_);
  Mapper::Serialize(data);
}

bool Mapper010::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&latch_0_)
      .ReadData(&latch_1_)
      .ReadData(&select_chr_first_)
      .ReadData(&select_chr_second_)
      .ReadData(&chr_regs_[0])
      .ReadData(&chr_regs_[1])
      .ReadData(&chr_regs_[2])
      .ReadData(&chr_regs_[3])
      .ReadData(&mirroring_)
      .ReadData(&select_prg_)
      .ReadData(&bg_chr_change_countdown_)
      .ReadData(&delayed_chr_bank_index_);
  mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
