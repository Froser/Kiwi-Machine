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

#include <SDL.h>
#include <imgui.h>
#include <kiwi_nes.h>
#include <array>

#include "build/kiwi_defines.h"
#include "preset_roms/preset_roms.h"
#include "resources/font_resources.h"
#include "resources/string_resources.h"
#include "ui/application.h"
#include "utility/localization.h"
#include "utility/zip_reader.h"

namespace {
std::array<ImFont*, static_cast<int>(FontType::kMax)> g_fonts;

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

int g_frame_count = 0;

std::set<ImWchar> g_chars;

void OnGetFallbackGlyph(ImWchar c) {
  int frame_count = ImGui::GetFrameCount();
  g_chars.insert(c);

  bool frame_changed = g_frame_count != frame_count && frame_count != 0;
  if (frame_changed) {
    bool has_new_chars = false;
    for (ImWchar w : g_chars) {
      if (AddCharToGlyphRanges(w))
        has_new_chars = true;
    }

    if (has_new_chars) {
      auto task_runner = kiwi::base::SequencedTaskRunner::GetCurrentDefault();
      task_runner->PostTask(FROM_HERE, kiwi::base::BindOnce(&InitializeFonts));
    }
    g_chars.clear();
    g_frame_count = frame_count;
  }
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

void RegisterSysFont(FontType font_begin, FontType font_end, int basic_size) {
  for (FontType ft = font_begin; ft <= font_end;
       ft = static_cast<FontType>(static_cast<int>(ft) + 1)) {
    int font_size =
        basic_size * (static_cast<int>(ft) - static_cast<int>(font_begin) + 1);
    ImFontConfig cfg;
    cfg.SizePixels = font_size;
    g_fonts[static_cast<int>(ft)] = ImGui::GetIO().Fonts->AddFontDefault(&cfg);
  }
}

void RegisterFont(FontType font_begin,
                  FontType font_end,
                  font_resources::FontID font_id,
                  int basic_size,
                  const ImWchar* glyph_ranges) {
  ImFontConfig font_config;
  font_config.FontDataOwnedByAtlas = false;
  for (FontType ft = font_begin; ft <= font_end;
       ft = static_cast<FontType>(static_cast<int>(ft) + 1)) {
    int font_size =
        basic_size * (static_cast<int>(ft) - static_cast<int>(font_begin) + 1);
    size_t data_size;
    const auto* font_data = const_cast<unsigned char*>(
        font_resources::GetData(font_id, &data_size));
    g_fonts[static_cast<int>(ft)] = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(font_data), data_size, font_size,
        &font_config, glyph_ranges);

    g_fonts[static_cast<int>(ft)]->SetOnGetFallbackGlyph(&OnGetFallbackGlyph);
  }
}

void InitializeSystemFonts() {
  RegisterSysFont(FontType::kSystemDefault, FontType::kSystemDefault3x, 13);
}

void InitializeFonts() {
  ImGui::GetIO().Fonts->Clear();
  InitializeSystemFonts();

#if !DISABLE_CHINESE_FONT
  ImVector<ImWchar> glyph_ranges_zh =
      GetGlyphRanges(SupportedLanguage::kSimplifiedChinese);
  RegisterFont(FontType::kDefaultSimplifiedChinese,
               FontType::kDefaultSimplifiedChinese3x,
               font_resources::FontID::kDengb, 16, glyph_ranges_zh.begin());
#endif
#if !DISABLE_JAPANESE_FONT
  ImVector<ImWchar> glyph_ranges_ja =
      GetGlyphRanges(SupportedLanguage::kJapanese);
  RegisterFont(FontType::kDefaultJapanese, FontType::kDefaultJapanese3x,
               font_resources::FontID::kYumindb, 16, glyph_ranges_ja.begin());
#endif
  RegisterFont(FontType::kDefault, FontType::kDefault3x,
               font_resources::FontID::kSupermario256, 16, NULL);

  bool success = ImGui::GetIO().Fonts->Build();
  SDL_assert(success);

  Application::Get()->FontChanged();
}

FontType GetPreferredFontType(PreferredFontSize size, FontType default_type) {
  switch (GetCurrentSupportedLanguage()) {
#if !DISABLE_CHINESE_FONT
    case SupportedLanguage::kSimplifiedChinese:
      return (static_cast<FontType>(
          static_cast<int>(FontType::kDefaultSimplifiedChinese) +
          static_cast<int>(size)));
#endif
#if !DISABLE_JAPANESE_FONT
    case SupportedLanguage::kJapanese:
      return (
          static_cast<FontType>(static_cast<int>(FontType::kDefaultJapanese) +
                                static_cast<int>(size)));
#endif
    default:
      return (static_cast<FontType>(static_cast<int>(default_type) +
                                    static_cast<int>(size)));
  }
}

ScopedFont GetPreferredFont(PreferredFontSize size, FontType default_type) {
  return ScopedFont(GetPreferredFontType(size, default_type));
}

FontType GetPreferredFontType(PreferredFontSize size,
                              const char* text_hint,
                              FontType default_type) {
  bool is_ascii = IsASCIIString(text_hint);
  if (is_ascii)
    return (static_cast<FontType>(static_cast<int>(default_type) +
                                  static_cast<int>(size)));

  switch (GetCurrentSupportedLanguage()) {
#if !DISABLE_CHINESE_FONT
    case SupportedLanguage::kSimplifiedChinese:
      return (static_cast<FontType>(
          static_cast<int>(FontType::kDefaultSimplifiedChinese) +
          static_cast<int>(size)));
#endif
#if !DISABLE_JAPANESE_FONT
    case SupportedLanguage::kJapanese:
      return (
          static_cast<FontType>(static_cast<int>(FontType::kDefaultJapanese) +
                                static_cast<int>(size)));
#endif
    default:
      return (static_cast<FontType>(static_cast<int>(default_type) +
                                    static_cast<int>(size)));
  }
}

ScopedFont GetPreferredFont(PreferredFontSize size,
                            const char* text_hint,
                            FontType default_type) {
  return ScopedFont(GetPreferredFontType(size, text_hint, default_type));
}