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

#include "nes/mappers/mapper009.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

constexpr size_t kPRGSize = 128 * 1024;
constexpr size_t kPRGBankSize = 8 * 1024;
constexpr size_t kPRGBankCount = kPRGSize / kPRGBankSize;

constexpr size_t kCHRSize = 128 * 1024;
constexpr size_t kCHRBankSize = 4 * 1024;
constexpr size_t kCHRBankCount = kCHRSize / kCHRBankSize;

Mapper009::Mapper009(Cartridge* cartridge) : Mapper(cartridge) {
  // Mapper009 has 128KB PRG and 128KB CHR.
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

Mapper009::~Mapper009() = default;

void Mapper009::WritePRG(Address address, Byte value) {
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

Byte Mapper009::ReadPRG(Address address) {
  // CPU $8000-$9FFF: 8 KB switchable PRG ROM bank
  if (address < 0xa000) {
    size_t banks = select_prg_ % kPRGBankCount;
    return rom_data()->PRG[kPRGBankSize * banks + (address - 0x8000)];
  }

  // CPU $A000-$FFFF: Three 8 KB PRG ROM banks, fixed to the last three banks
  return rom_data()
      ->PRG[kPRGBankSize * (kPRGBankCount - 3) + (address - 0xa000)];
}

void Mapper009::WriteCHR(Address address, Byte value) {}

Byte Mapper009::ReadCHR(Address address) {
  if ((address & 0x1ff0) == 0x0fd0 && latch_0_ != 0xfd) {
    latch_0_ = 0xfd;
    select_chr_first_ = chr_regs_[0];
  } else if ((address & 0x1ff0) == 0x0fe0 && latch_0_ != 0xfe) {
    latch_0_ = 0xfe;
    select_chr_first_ = chr_regs_[1];
  } else if ((address & 0x1ff0) == 0x1fd0 && latch_1_ != 0xfd) {
    latch_1_ = 0xfd;
    select_chr_second_ = chr_regs_[2];
  } else if ((address & 0x1ff0) == 0x1fe0 && latch_1_ != 0xfe) {
    latch_1_ = 0xfe;
    select_chr_second_ = chr_regs_[3];
  }

  switch (address & 0xf000) {
    case 0x0000:
      return rom_data()
          ->CHR[select_chr_first_ * kCHRBankSize + (address & 0x0fff)];
    case 0x1000:
      return rom_data()
          ->CHR[select_chr_second_ * kCHRBankSize + (address & 0x0fff)];
    default:
      CHECK(false) << "Shouldn't be here";
      return 0;
  }
}

NametableMirroring Mapper009::GetNametableMirroring() {
  return mirroring_;
}

void Mapper009::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(latch_0_)
      .WriteData(latch_1_)
      .WriteData(select_chr_first_)
      .WriteData(select_chr_second_)
      .WriteData(chr_regs_[0])
      .WriteData(chr_regs_[1])
      .WriteData(chr_regs_[2])
      .WriteData(chr_regs_[3])
      .WriteData(mirroring_)
      .WriteData(select_prg_);
  Mapper::Serialize(data);
}

bool Mapper009::Deserialize(const EmulatorStates::Header& header,
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
      .ReadData(&select_prg_);
  mirroring_changed_callback().Run();
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
