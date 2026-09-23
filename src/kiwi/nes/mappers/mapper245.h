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

#ifndef NES_MAPPERS_MAPPER245_H_
#define NES_MAPPERS_MAPPER245_H_

#include "nes/mappers/mapper004.h"

namespace kiwi {
namespace nes {

// Waixing F003: MMC3 with CHR A11 connected to PRG A19.
// https://www.nesdev.org/wiki/INES_Mapper_245
class Mapper245 final : public Mapper004 {
 public:
  explicit Mapper245(Cartridge* cartridge);
  ~Mapper245() override;

  Byte ReadPRG(Address address) override;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER245_H_
