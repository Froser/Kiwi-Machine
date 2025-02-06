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

#include "nes/mappers/mapper003.h"

#include "base/check.h"
#include "base/logging.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
Mapper003::Mapper003(Cartridge* cartridge) : Mapper(cartridge) {
  if (cartridge->GetRomData()->submapper != 0) {
    LOG(WARNING) << "The cartridge's submapper is "
                 << cartridge->GetRomData()->submapper
                 << ", which may have subtle problems.";
  }

  if (cartridge->GetRomData()->PRG.size() == 0x4000) {
    is_one_bank_ = true;
  } else {
    is_one_bank_ = false;
  }
}

Mapper003::~Mapper003() = default;

// D~[..DC ..BA] A~[1... .... .... ....]
//      ||   ||
//      ||   ++- CHR A14..A13 (8 KiB bank)
//      |+------ Output to Diode 2 (D2)
//      +------- Output to Diode 1 (D1)
void Mapper003::WritePRG(Address address, Byte value) {
  if (address >= 0x8000)
    select_chr_ = value & 0x3;
}

Byte Mapper003::ReadPRG(Address address) {
  if (!is_one_bank_)
    return cartridge()->GetRomData()->PRG.at(address - 0x8000);

  return cartridge()->GetRomData()->PRG.at((address - 0x8000) & 0x3fff);
}

void Mapper003::WriteCHR(Address address, Byte value) {
  LOG(ERROR) << "CHR read-only.";
}

Byte Mapper003::ReadCHR(Address address) {
  constexpr uint32_t kCHRBankSize = 0x2000;  // 8 KB switchable PRG bank

  // Some games will set a wrong bank (more than banks). For example: Tetris
  // (Tengen). To solve this, the bank should be clamped.
  uint32_t banks = cartridge()->GetRomData()->CHR.size() / kCHRBankSize;
  return cartridge()
      ->GetRomData()
      ->CHR[(kCHRBankSize * (select_chr_ % banks)) | (address & 0x1fff)];
}

void Mapper003::Serialize(EmulatorStates::SerializableStateData& data) {
  data.WriteData(select_chr_);
  Mapper::Serialize(data);
}

bool Mapper003::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  data.ReadData(&select_chr_);
  return Mapper::Deserialize(header, data);
}

}  // namespace nes
}  // namespace kiwi
