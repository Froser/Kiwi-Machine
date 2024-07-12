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

#ifndef UTILITY_ZIP_READER_H_
#define UTILITY_ZIP_READER_H_

#include <kiwi_nes.h>

namespace preset_roms {
struct PresetROM;
}

enum class RomPart {
  kNone,
  kCover = 1,
  kContent = 2,
  kAll = kCover | kContent,
};
inline RomPart operator&(RomPart lhs, RomPart rhs) {
  return static_cast<RomPart>(static_cast<int>(lhs) & static_cast<int>(rhs));
}
inline RomPart operator|(RomPart lhs, RomPart rhs) {
  return static_cast<RomPart>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
inline RomPart& operator|=(RomPart& lhs, RomPart rhs) {
  lhs = static_cast<RomPart>(static_cast<int>(lhs) | static_cast<int>(rhs));
  return lhs;
}
inline bool HasAnyPart(RomPart part) {
  return !!static_cast<int>(part);
}

#if defined(KIWI_USE_EXTERNAL_PAK)
// Loads ROM's data from an external package. This function should be called
// before InitializePresetROM().
void OpenRomDataFromPackage(std::vector<preset_roms::PresetROM>& roms,
                            const kiwi::base::FilePath& package);

void CloseRomDataFromPackage(std::vector<preset_roms::PresetROM>& roms);

#endif

// Loads all ROM's title, i18n names, and alternative titles. This function
// should be called before calling LoadPresetROM().
void InitializePresetROM(preset_roms::PresetROM& rom_data);

// Loads ROM's cover, content, or both. This function will be called on IO
// thread.
void LoadPresetROM(preset_roms::PresetROM& rom_data, RomPart part);

#endif  // UTILITY_ZIP_READER_H_