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

#include <imgui.h>
#include <kiwi_nes.h>

#include "resources/font_resources.h"
#include "resources/string_resources.h"
#include "utility/localization.h"

namespace {
ImVector<ImWchar> g_glyph_ranges;

ImFont* g_fonts[static_cast<int>(FontType::kMax)];

ImFont* GetFont(FontType type) {
  return g_fonts[static_cast<int>(type)];
}

bool IsASCIIString(const char* str) {
  const char* p = str;
  while (*p) {
    if (static_cast<unsigned char>(*p) > 127)
      return false;
    ++p;
  }
  return true;
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

#define REGISTER_SYS_FONT(enumName, basicSize)                                 \
  {                                                                            \
    for (FontType ft = enumName; ft <= enumName##6x;                           \
         ft = static_cast<FontType>(static_cast<int>(ft) + 1)) {               \
      int font_size =                                                          \
          basicSize * (static_cast<int>(ft) - static_cast<int>(enumName) + 1); \
      ImFontConfig cfg;                                                        \
      cfg.SizePixels = font_size;                                              \
      g_fonts[static_cast<int>(ft)] =                                          \
          ImGui::GetIO().Fonts->AddFontDefault(&cfg);                          \
    }                                                                          \
  }

#define REGISTER_FONT(enumName, fontType, basicSize, glyph_ranges)             \
  {                                                                            \
    ImFontConfig font_config;                                                  \
    font_config.FontDataOwnedByAtlas = false;                                  \
    for (FontType ft = enumName; ft <= enumName##6x;                           \
         ft = static_cast<FontType>(static_cast<int>(ft) + 1)) {               \
      int font_size =                                                          \
          basicSize * (static_cast<int>(ft) - static_cast<int>(enumName) + 1); \
      size_t data_size;                                                        \
      g_fonts[static_cast<int>(ft)] =                                          \
          ImGui::GetIO().Fonts->AddFontFromMemoryTTF(                          \
              const_cast<unsigned char*>(                                      \
                  font_resources::GetData(fontType, &data_size)),              \
              data_size, font_size, &font_config, (glyph_ranges));             \
    }                                                                          \
  }

void InitializeFonts() {
  // Builds all ranges for all IDR.
  ImFontGlyphRangesBuilder ranges_builder;
  for (int i = 0; i < string_resources::END_OF_STRINGS; ++i) {
    ranges_builder.AddText(GetLocalizedString(i).c_str());
  }
  ranges_builder.BuildRanges(&g_glyph_ranges);

  REGISTER_SYS_FONT(FontType::kSystemDefault, 13);
  REGISTER_FONT(FontType::kStxihei, font_resources::FontID::kStxihei, 16,
                g_glyph_ranges.begin());
  REGISTER_FONT(FontType::kDefault, font_resources::FontID::kSupermario256, 16,
                NULL);
}

FontType GetPreferredFontType(PreferredFontSize size,
                              const char* text_hint,
                              FontType default_type) {
  bool is_ascii = IsASCIIString(text_hint);
  if (is_ascii)
    return (static_cast<FontType>(static_cast<int>(default_type) +
                                  static_cast<int>(size)));

  if (kiwi::base::CompareCaseInsensitiveASCII(GetLanguage(), "zh") == 0) {
    return (static_cast<FontType>(static_cast<int>(FontType::kStxihei) +
                                  static_cast<int>(size)));
  }

  return (static_cast<FontType>(static_cast<int>(default_type) +
                                static_cast<int>(size)));
}

ScopedFont GetPreferredFont(PreferredFontSize size,
                            const char* text_hint,
                            FontType default_type) {
  return ScopedFont(GetPreferredFontType(size, text_hint, default_type));
}