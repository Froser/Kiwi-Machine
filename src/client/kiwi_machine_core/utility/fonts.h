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
  kSystemDefault,
  kSystemDefault2x,
  kSystemDefault3x,
  kSystemDefault4x,
  kSystemDefault5x,
  kSystemDefault6x,

  kDefault,
  kDefault2x,
  kDefault3x,
  kDefault4x,
  kDefault5x,
  kDefault6x,

  kStxihei,
  kStxihei2x,
  kStxihei3x,
  kStxihei4x,
  kStxihei5x,
  kStxihei6x,

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

enum PreferredFontSize {
  k1x,
  k2x,
  k3x,
  k4x,
  k5x,
  k6x,
};

FontType GetPreferredFontType(PreferredFontSize size,
                              const char* text_hint,
                              FontType default_type = FontType::kDefault);
ScopedFont GetPreferredFont(PreferredFontSize size,
                            const char* text_hint,
                            FontType default_type = FontType::kDefault);

#endif  // UTILITY_FONTS_H_