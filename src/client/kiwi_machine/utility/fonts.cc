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

#include "utility/fonts.h"

namespace {

ImFont* g_fonts[static_cast<int>(FontType::kMax)];

ImFont* GetFont(FontType type) {
  return g_fonts[static_cast<int>(type)];
}

}  // namespace

ScopedFont::ScopedFont(FontType font) : type_(font) {
  ImGui::PushFont(::GetFont(font));
}

ScopedFont::~ScopedFont() {
  ImGui::PopFont();
}

ImFont* ScopedFont::GetFont() {
  return g_fonts[static_cast<int>(type_)];
}

void InitializeFonts() {
  g_fonts[static_cast<int>(FontType::kDefault)] =
      ImGui::GetIO().Fonts->AddFontDefault();

  ImFontConfig default_font_config_2x;
  default_font_config_2x.SizePixels = 26;
  g_fonts[static_cast<int>(FontType::kDefault2x)] =
      ImGui::GetIO().Fonts->AddFontDefault(&default_font_config_2x);
}