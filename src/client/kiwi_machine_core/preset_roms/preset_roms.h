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
#include <map>
#include <unordered_map>
#include <vector>
#include <gflags/gflags.h>

#include "utility/localization.h"
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"

namespace preset_roms {
enum class Region {
  kUnknown,
  kJapan,
  kUSA,
  kCN, // Rare, almost bootleg
};

struct PresetROM {
  const char* name;

  unz_file_pos file_pos;
  kiwi::base::RepeatingCallback<kiwi::nes::Bytes(unz_file_pos)> zip_data_loader;

  // Following data will be written when loaded. Do not modify them:
  // Uncompressed data. Filled by FillRomDataFromZip().
  kiwi::nes::Bytes rom_data;
  kiwi::nes::Bytes rom_cover;

  // i18 names
  std::unordered_map<std::string, std::string> i18n_names;

  // Switch ROM version.
  std::vector<PresetROM> alternates;

  // ROM's region
  Region region = Region::kUnknown;

  // Whether its data or cover is loaded
  bool title_loaded = false;
  bool cover_loaded = false;
  bool content_loaded = false;
  bool owned_zip_data = true;
};

#define PRESET_ROM(name) name::ROM_NAME, name::ROM_ZIP, name::ROM_ZIP_SIZE

#define EXTERN_ROM(name)                  \
  namespace name {                        \
  extern const char ROM_NAME[];           \
  extern const kiwi::nes::Byte ROM_ZIP[]; \
  extern size_t ROM_ZIP_SIZE;             \
  }

struct Package {
  Package() = default;
  virtual ~Package() = default;
  virtual size_t GetRomsCount() = 0;
  virtual PresetROM& GetRomsByIndex(size_t index) = 0;
  virtual kiwi::nes::Bytes GetSideMenuImage() = 0;
  virtual kiwi::nes::Bytes GetSideMenuHighlightImage() = 0;
  virtual std::string GetTitleForLanguage(SupportedLanguage language) = 0;
};

std::vector<Package*> GetPresetOrTestRomsPackages();

}  // namespace preset_roms

#endif  // PRESET_ROMS_PRESET_ROMS_H
