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
#include <array>

#include "build/kiwi_defines.h"
#include "resources/font_resources.h"
#include "utility/localization.h"

namespace {
constexpr float kSystemDefaultFontSize = 13.f;
constexpr float kDefaultFontSize = 16.f;

std::array<ImFont*, static_cast<int>(FontType::kMax)> g_fonts;

ImFont* GetFont(FontType type) {
  ImFont* font = g_fonts[static_cast<int>(type)];
  IM_ASSERT(font);
  return font;
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

float GetFontSize(ImFont* font, PreferredFontSize size) {
  return font->LegacySize * static_cast<int>(size);
}

FontType GetPreferredFontType(FontType default_type) {
  switch (GetCurrentSupportedLanguage()) {
#if !DISABLE_CHINESE_FONT
    case SupportedLanguage::kSimplifiedChinese:
      return FontType::kDefaultSimplifiedChinese;
#endif
#if !DISABLE_JAPANESE_FONT
    case SupportedLanguage::kJapanese:
      return FontType::kDefaultJapanese;
#endif
    default:
      return default_type;
  }
}

FontType GetPreferredFontType(const char* text_hint, FontType default_type) {
  return IsASCIIString(text_hint) ? default_type
                                 : GetPreferredFontType(default_type);
}

void RegisterSystemFont() {
  ImFontConfig font_config;
  font_config.SizePixels = kSystemDefaultFontSize;
  g_fonts[static_cast<int>(FontType::kSystemDefault)] =
      ImGui::GetIO().Fonts->AddFontDefaultVector(&font_config);
}

void RegisterFont(FontType type,
                  font_resources::FontID font_id,
                  float font_size) {
  ImFontConfig font_config;
  font_config.FontDataOwnedByAtlas = false;
  size_t data_size;
  auto* font_data = const_cast<unsigned char*>(
      font_resources::GetData(font_id, &data_size));
  g_fonts[static_cast<int>(type)] =
      ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
          font_data, data_size, font_size, &font_config);
}

}  // namespace

ScopedFont::ScopedFont(FontType font, PreferredFontSize size) : type_(font) {
  ImFont* resolved_font = ::GetFont(font);
  ImGui::PushFont(resolved_font, ::GetFontSize(resolved_font, size));
  font_size_ = ImGui::GetFontSize();
}

ScopedFont::~ScopedFont() {
  ImGui::PopFont();
}

ImFont* ScopedFont::GetFont() const {
  return ::GetFont(type_);
}

float ScopedFont::GetFontSize() const {
  return font_size_;
}

void InitializeSystemFonts() {
  RegisterSystemFont();
}

void InitializeFonts() {
  ImGui::GetIO().Fonts->Clear();
  InitializeSystemFonts();

#if !DISABLE_CHINESE_FONT
  RegisterFont(FontType::kDefaultSimplifiedChinese,
               font_resources::FontID::kDengb, kDefaultFontSize);
#endif
#if !DISABLE_JAPANESE_FONT
  RegisterFont(FontType::kDefaultJapanese,
               font_resources::FontID::kYumindb, kDefaultFontSize);
#endif
  RegisterFont(FontType::kDefault, font_resources::FontID::kSupermario256,
               kDefaultFontSize);
}

ScopedFont GetPreferredFont(PreferredFontSize size, FontType default_type) {
  return ScopedFont(GetPreferredFontType(default_type), size);
}

ScopedFont GetPreferredFont(PreferredFontSize size,
                            const char* text_hint,
                            FontType default_type) {
  return ScopedFont(GetPreferredFontType(text_hint, default_type), size);
}
