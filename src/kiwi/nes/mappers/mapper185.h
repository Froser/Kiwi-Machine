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

#ifndef NES_MAPPERS_MAPPER185_H_
#define NES_MAPPERS_MAPPER185_H_

#include "nes/mappers/mapper003.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
// https://www.nesdev.org/wiki/CNROM
class Mapper185 : public Mapper003 {
 public:
  explicit Mapper185(Cartridge* cartridge);
  ~Mapper185() override;
};

}  // namespace core
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER185_H_
