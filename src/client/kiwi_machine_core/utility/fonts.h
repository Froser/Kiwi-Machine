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
  kDefault,
  kDefaultSimplifiedChinese,
  kDefaultJapanese,
  kMax,
};

// Multipliers applied to each font's registered default size.
enum class PreferredFontSize {
  k1x = 1,
  k2x,
  k3x,
  k4x,
  k5x,
  k6x,
};

class ScopedFont {
 public:
  explicit ScopedFont(FontType font,
                      PreferredFontSize size = PreferredFontSize::k1x);
  ~ScopedFont();

  ScopedFont(const ScopedFont&) = delete;
  ScopedFont& operator=(const ScopedFont&) = delete;

  FontType type() const { return type_; }
  ImFont* GetFont() const;
  float GetFontSize() const;

 private:
  FontType type_;
  float font_size_ = 0.f;
};

void InitializeSystemFonts();
void InitializeFonts();
ScopedFont GetPreferredFont(PreferredFontSize size,
                            FontType default_type = FontType::kDefault);
ScopedFont GetPreferredFont(PreferredFontSize size,
                            const char* text_hint,
                            FontType default_type = FontType::kDefault);

#endif  // UTILITY_FONTS_H_
