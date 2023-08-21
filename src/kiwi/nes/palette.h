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

#ifndef NES_PALETTE_H_
#define NES_PALETTE_H_

#include "nes/nes_export.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
class NES_EXPORT Palette {
 public:
  Palette();
  virtual ~Palette();

  virtual Color GetColorBGRA(int index) = 0;
};

enum class PPUModel {
  k2C02,
};

std::unique_ptr<Palette> CreatePaletteFromPPUModel(PPUModel ppu);
}  // namespace core
}  // namespace kiwi

#endif  // NES_PALETTE_H_
