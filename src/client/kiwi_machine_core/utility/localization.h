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

#ifndef UTILITY_LOCALIZATION_H_
#define UTILITY_LOCALIZATION_H_

#include <imgui.h>
#include <string>

#include "resources/string_resources.h"

namespace preset_roms {
struct PresetROM;
}  // namespace preset_roms

enum class SupportedLanguage {
  kEnglish,
  kSimplifiedChinese,
  kJapanese,

  kMax,
};

class LocalizedStringUpdater {
 public:
  LocalizedStringUpdater();
  virtual ~LocalizedStringUpdater();

 public:
  virtual std::string GetLocalizedString() = 0;
};

void SetLanguage(const char* language);

void SetLanguage(SupportedLanguage language);

SupportedLanguage GetCurrentSupportedLanguage();

const char* GetLanguage();

const char* GetROMLocalizedTitle(const preset_roms::PresetROM& rom);

const std::string& GetLocalizedString(int id);

const ImVector<ImWchar>& GetGlyphRanges(SupportedLanguage language);

#endif  // UTILITY_LOCALIZATION_H_