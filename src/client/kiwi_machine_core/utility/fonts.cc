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
#if KIWI_SWITCH
#include <SDL_log.h>
extern "C" {
#include <switch/result.h>
#include <switch/services/pl.h>
}
#else
#include "resources/font_resources.h"
#endif
#include "utility/localization.h"

namespace {
constexpr float kSystemDefaultFontSize = 13.f;
constexpr float kDefaultFontSize = 16.f;

std::array<ImFont*, static_cast<int>(FontType::kMax)> g_fonts;

ImFont* GetFont(FontType type) {
  ImFont* font = g_fonts[static_cast<int>(type)];
  if (!font) {
    font = g_fonts[static_cast<int>(FontType::kSystemDefault)];
  }
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

#if KIWI_SWITCH
ImFont* RegisterSwitchSharedFont(PlSharedFontType font_type,
                                 float font_size,
                                 ImFont* merge_target = nullptr) {
  PlFontData font_data = {};
  const Result result = plGetSharedFontByType(&font_data, font_type);
  if (R_FAILED(result) || !font_data.address || !font_data.size) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to load Switch shared font %d: 0x%08x",
                 static_cast<int>(font_type), result);
    return nullptr;
  }

  ImFontConfig font_config;
  // pl:u owns the mapped font data and remains alive until ImGui is destroyed.
  font_config.FontDataOwnedByAtlas = false;
  font_config.MergeMode = merge_target != nullptr;
  font_config.DstFont = merge_target;
  return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
      font_data.address, static_cast<int>(font_data.size), font_size,
      &font_config);
}

void RegisterSwitchFonts() {
  ImFont* standard_font =
      RegisterSwitchSharedFont(PlSharedFontType_Standard, kDefaultFontSize);
  if (!standard_font) {
    RegisterSystemFont();
    standard_font = g_fonts[static_cast<int>(FontType::kSystemDefault)];
  }

  g_fonts[static_cast<int>(FontType::kSystemDefault)] = standard_font;
  g_fonts[static_cast<int>(FontType::kDefault)] = standard_font;
  g_fonts[static_cast<int>(FontType::kDefaultJapanese)] = standard_font;

  ImFont* simplified_chinese_font = RegisterSwitchSharedFont(
      PlSharedFontType_ChineseSimplified, kDefaultFontSize);
  if (simplified_chinese_font) {
    RegisterSwitchSharedFont(PlSharedFontType_ExtChineseSimplified,
                             kDefaultFontSize, simplified_chinese_font);
  }
  g_fonts[static_cast<int>(FontType::kDefaultSimplifiedChinese)] =
      simplified_chinese_font ? simplified_chinese_font : standard_font;
}
#else
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
#endif

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
#if KIWI_SWITCH
  RegisterSwitchFonts();
#else
  RegisterSystemFont();
#endif
}

void InitializeStartupFonts() {
  ImGui::GetIO().Fonts->Clear();
  g_fonts.fill(nullptr);
  InitializeSystemFonts();
#if !KIWI_SWITCH
  RegisterFont(FontType::kDefault, font_resources::FontID::kSupermario256,
               kDefaultFontSize);

  switch (GetCurrentSupportedLanguage()) {
#if !DISABLE_CHINESE_FONT
    case SupportedLanguage::kSimplifiedChinese:
      RegisterFont(FontType::kDefaultSimplifiedChinese,
                   font_resources::FontID::kDengb, kDefaultFontSize);
      break;
#endif
#if !DISABLE_JAPANESE_FONT
    case SupportedLanguage::kJapanese:
      RegisterFont(FontType::kDefaultJapanese, font_resources::FontID::kYumindb,
                   kDefaultFontSize);
      break;
#endif
    default:
      break;
  }
#endif
}

void InitializeFonts() {
  ImGui::GetIO().Fonts->Clear();
  g_fonts.fill(nullptr);
  InitializeSystemFonts();

#if !KIWI_SWITCH
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
#endif
}

ScopedFont GetPreferredFont(PreferredFontSize size, FontType default_type) {
  return ScopedFont(GetPreferredFontType(default_type), size);
}

ScopedFont GetPreferredFont(PreferredFontSize size,
                            const char* text_hint,
                            FontType default_type) {
  return ScopedFont(GetPreferredFontType(text_hint, default_type), size);
}
