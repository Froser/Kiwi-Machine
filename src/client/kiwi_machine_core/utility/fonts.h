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

  kDefault,
  kDefault2x,
  kDefault3x,

  kDefaultSimplifiedChinese,
  kDefaultSimplifiedChinese2x,
  kDefaultSimplifiedChinese3x,

  kDefaultJapanese,
  kDefaultJapanese2x,
  kDefaultJapanese3x,

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

void InitializeSystemFonts();
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
                              FontType default_type = FontType::kDefault);
ScopedFont GetPreferredFont(PreferredFontSize size,
                            FontType default_type = FontType::kDefault);
FontType GetPreferredFontType(PreferredFontSize size,
                              const char* text_hint,
                              FontType default_type = FontType::kDefault);
ScopedFont GetPreferredFont(PreferredFontSize size,
                            const char* text_hint,
                            FontType default_type = FontType::kDefault);

#endif  // UTILITY_FONTS_H_