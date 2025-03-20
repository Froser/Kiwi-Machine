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

#ifndef NES_MAPPERS_MAPPER033_H_
#define NES_MAPPERS_MAPPER033_H_

#include "nes/mappers/mapper048.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
class Mapper033 : public Mapper048 {
 public:
  explicit Mapper033(Cartridge* cartridge);
  ~Mapper033() override;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER033_H_
