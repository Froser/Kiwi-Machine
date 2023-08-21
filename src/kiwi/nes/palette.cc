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

#include "nes/palette.h"

#include "base/check.h"

namespace kiwi {
namespace nes {
namespace {
class Palette_2C02 : public Palette {
 public:
  Palette_2C02();
  ~Palette_2C02() override;

  Color GetColorBGRA(int index) override;
};

Palette_2C02::Palette_2C02() = default;
Palette_2C02::~Palette_2C02() = default;

Color Palette_2C02::GetColorBGRA(int index) {
  static const Color s_colors_bgra[] = {
      0xff666666, 0xff002a88, 0xff1412a7, 0xff3b00a4, 0xff5c007e, 0xff6e0040,
      0xff6c0600, 0xff561d00, 0xff333500, 0xff0b4800, 0xff005200, 0xff004f08,
      0xff00404d, 0xff000000, 0xff000000, 0xff000000, 0xffadadad, 0xff155fd9,
      0xff4240ff, 0xff7527fe, 0xffa01acc, 0xffb71e7b, 0xffb53120, 0xff994e00,
      0xff6b6d00, 0xff388700, 0xff0c9300, 0xff008f32, 0xff007c8d, 0xff000000,
      0xff000000, 0xff000000, 0xfffffeff, 0xff64b0ff, 0xff9290ff, 0xffc676ff,
      0xfff36aff, 0xfffe6ecc, 0xfffe8170, 0xffea9e22, 0xffbcbe00, 0xff88d800,
      0xff5ce430, 0xff45e082, 0xff48cdde, 0xff4f4f4f, 0xff000000, 0xff000000,
      0xfffffeff, 0xffc0dfff, 0xffd3d2ff, 0xffe8c8ff, 0xfffbc2ff, 0xfffec4ea,
      0xfffeccc5, 0xfff7d8a5, 0xffe4e594, 0xffcfef96, 0xffbdf4ab, 0xffb3f3cc,
      0xffb5ebf2, 0xffb8b8b8, 0xff000000, 0xff000000,
  };

  DCHECK(index >= 0 && index <= sizeof(s_colors_bgra) / sizeof(Color));
  return s_colors_bgra[index];
}

}  // namespace

Palette::Palette() = default;
Palette::~Palette() = default;
std::unique_ptr<Palette> CreatePaletteFromPPUModel(PPUModel ppu) {
  return std::make_unique<Palette_2C02>();
}
}  // namespace core
}  // namespace kiwi
