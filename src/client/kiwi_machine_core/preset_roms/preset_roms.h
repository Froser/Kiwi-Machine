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

#ifndef PRESET_ROMS_PRESET_ROMS_H
#define PRESET_ROMS_PRESET_ROMS_H

#include <kiwi_nes.h>
#include <unordered_map>

namespace preset_roms {
struct PresetROM {
  const char* name;
  const kiwi::nes::Byte* zip_data;
  size_t zip_size;

  // Uncompressed data. Filled by FillRomDataFromZip().
  mutable kiwi::nes::Bytes rom_data;
  mutable kiwi::nes::Bytes rom_cover;

  // i18 names
  mutable std::unordered_map<std::string, std::string> i18n_names;

  // Switch ROM version.
  mutable std::vector<PresetROM> alternates;

  // Whether its data or cover is loaded
  mutable bool loaded = false;
};

#define PRESET_ROM(name) name::ROM_NAME, name::ROM_ZIP, name::ROM_ZIP_SIZE

#define EXTERN_ROM(name)                  \
  namespace name {                        \
  extern const char ROM_NAME[];           \
  extern const kiwi::nes::Byte ROM_ZIP[]; \
  extern size_t ROM_ZIP_SIZE;             \
  }

#if !defined(KIWI_USE_EXTERNAL_PAK)
const PresetROM* GetPresetRoms();
#else
std::vector<PresetROM>& GetPresetRoms();
const char* GetPresetRomsPackageName();
#endif
size_t GetPresetRomsCount();

namespace specials {
#if !defined(KIWI_USE_EXTERNAL_PAK)
const PresetROM* GetPresetRoms();
#else
std::vector<PresetROM>& GetPresetRoms();
const char* GetPresetRomsPackageName();
#endif
size_t GetPresetRomsCount();
}  // namespace specials

}  // namespace preset_roms

#endif  // PRESET_ROMS_PRESET_ROMS_H
