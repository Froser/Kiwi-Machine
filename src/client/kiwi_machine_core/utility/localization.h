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

#include <string>

#include "resources/string_resources.h"

namespace preset_roms {
class PresetROM;
}  // namespace preset_roms

void SetLanguage(const char* language);

const char* GetLanguage();

const char* GetROMLocalizedTitle(const preset_roms::PresetROM& rom);

const std::string& GetLocalizedString(int id);

#endif  // UTILITY_LOCALIZATION_H_