// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER113_H_
#define NES_MAPPERS_MAPPER113_H_

#include "nes/mappers/mapper079.h"

namespace kiwi {
namespace nes {

// NINA multicart variant with expanded PRG/CHR selection and mirroring.
// https://www.nesdev.org/wiki/INES_Mapper_113
class Mapper113 : public Mapper079 {
 public:
  explicit Mapper113(Cartridge* cartridge);
  ~Mapper113() override;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER113_H_
