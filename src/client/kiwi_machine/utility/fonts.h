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

#ifndef UTILITY_FONTS_H_
#define UTILITY_FONTS_H_

#include <imgui.h>

enum class FontType {
  kDefault,
  kDefault2x,
  kDefault3x,
  kDefault4x,
  kDefault5x,
  kDefault6x,

  kMax,
};

class ScopedFont {
 public:
  explicit ScopedFont(FontType font);
  ~ScopedFont();

  FontType type() { return type_; }
  ImFont* GetFont();

 private:
  FontType type_;
};

void InitializeFonts();

#endif  // UTILITY_FONTS_H_