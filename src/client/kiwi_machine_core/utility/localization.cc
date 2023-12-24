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

#include "utility/localization.h"

#include <SDL.h>
#include <kiwi_nes.h>
#include <map>
#include <memory>

#include "preset_roms/preset_roms.h"
#include "resources/string_resources.h"
#include "utility/zip_reader.h"

namespace {
std::string g_global_language;

using GlyphRangePtr = std::unique_ptr<ImVector<ImWchar>>;
std::map<SupportedLanguage, GlyphRangePtr> g_glyph_ranges;

const char* ToLanguageCode(SupportedLanguage language) {
  switch (language) {
    case SupportedLanguage::kEnglish:
      return "en";
    case SupportedLanguage::kSimplifiedChinese:
      return "zh";
    case SupportedLanguage::kJapanese:
      return "ja";
    default:
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Wrong language type %d",
                  language);
      SDL_assert(false);
      return "en";
  }
}

const char* GetROMLocalizedTitle(SupportedLanguage language,
                                 const preset_roms::PresetROM& rom) {
  auto local_name_iter = rom.i18n_names.find(ToLanguageCode(language));
  if (local_name_iter != rom.i18n_names.end()) {
    return local_name_iter->second.c_str();
  }

  return rom.name;
}

const std::string& GetLocalizedString(SupportedLanguage language, int id) {
  const string_resources::StringMap& string_map =
      string_resources::GetGlobalStringMap();

  auto id_iter = string_map.find(id);
  SDL_assert(id_iter != string_map.end());

  const auto& i18n_strings = id_iter->second;
  const char* app_language = ToLanguageCode(language);
  auto lang_iter = i18n_strings.find(app_language);
  if (lang_iter == i18n_strings.end()) {
    lang_iter = i18n_strings.find("default");
    SDL_assert(lang_iter != i18n_strings.end());
  }

  return lang_iter->second;
}

void BuildGlyphRanges(SupportedLanguage language,
                      ImVector<ImWchar>& out_ranges) {
  ImFontGlyphRangesBuilder ranges_builder;
  for (int i = 0; i < string_resources::END_OF_STRINGS; ++i) {
    ranges_builder.AddText(GetLocalizedString(language, i).c_str());
  }
  for (size_t i = 0; i < preset_roms::GetPresetRomsCount(); ++i) {
    const auto& rom = preset_roms::GetPresetRoms()[i];
    FillRomDataFromZip(rom);
    ranges_builder.AddText(GetROMLocalizedTitle(language, rom));
    for (const auto& alter : rom.alternates) {
      ranges_builder.AddText(GetROMLocalizedTitle(language, alter));
    }
  }
  for (size_t i = 0; i < preset_roms::specials::GetPresetRomsCount(); ++i) {
    const auto& rom = preset_roms::specials::GetPresetRoms()[i];
    FillRomDataFromZip(rom);
    ranges_builder.AddText(GetROMLocalizedTitle(language, rom));
    for (const auto& alter : rom.alternates) {
      ranges_builder.AddText(GetROMLocalizedTitle(language, alter));
    }
  }
  ranges_builder.BuildRanges(&out_ranges);
}

}  // namespace

LocalizedStringUpdater::LocalizedStringUpdater() = default;
LocalizedStringUpdater::~LocalizedStringUpdater() = default;

void SetLanguage(const char* language) {
  if (language)
    g_global_language = language;
  else
    g_global_language.clear();
}

void SetLanguage(SupportedLanguage language) {
  switch (language) {
    case SupportedLanguage::kEnglish:
      SetLanguage("en");
      break;
    case SupportedLanguage::kSimplifiedChinese:
      SetLanguage("zh");
      break;
    case SupportedLanguage::kJapanese:
      SetLanguage("ja");
      break;
    default:
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Wrong language type %d",
                  language);
      SDL_assert(false);
      break;
  }
}

SupportedLanguage GetCurrentSupportedLanguage() {
  const char* lang = GetLanguage();
  if (kiwi::base::CompareCaseInsensitiveASCII(GetLanguage(), "zh") == 0)
    return SupportedLanguage::kSimplifiedChinese;

  if (kiwi::base::CompareCaseInsensitiveASCII(GetLanguage(), "ja") == 0)
    return SupportedLanguage::kJapanese;

  return SupportedLanguage::kEnglish;
}

const char* GetLanguage() {
  if (!g_global_language.empty())
    return g_global_language.c_str();

  SDL_Locale* locale = SDL_GetPreferredLocales();
  return locale->language;
}

const char* GetROMLocalizedTitle(const preset_roms::PresetROM& rom) {
  return GetROMLocalizedTitle(GetCurrentSupportedLanguage(), rom);
}

const std::string& GetLocalizedString(int id) {
  return GetLocalizedString(GetCurrentSupportedLanguage(), id);
}

const ImVector<ImWchar>& GetGlyphRanges(SupportedLanguage language) {
  auto iter = g_glyph_ranges.find(language);
  if (iter == g_glyph_ranges.end()) {
    GlyphRangePtr range = std::make_unique<ImVector<ImWchar>>();
    BuildGlyphRanges(language, *range);
    g_glyph_ranges[language] = std::move(range);
  }

  SDL_assert(g_glyph_ranges[language]);
  return *g_glyph_ranges[language];
}