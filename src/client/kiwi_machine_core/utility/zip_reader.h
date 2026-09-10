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
#include <memory>

namespace preset_roms {
enum class ROMEdition;
struct PresetROM;
struct Package;
}  // namespace preset_roms

class TextureRenderer;

// Loads ROM's data from an external package.
preset_roms::Package* CreatePackageFromFile(
    const kiwi::base::FilePath& package_path);

// Loads ROM's data from an external package. This function should be called
// before InitializePresetROM().
void OpenPackageFromFile(const kiwi::base::FilePath& package_path);
void ClosePackages();

// Loads all ROM's title, i18n names, and alternative titles. This function
// should be called before calling LoadPresetROM().
void InitializePresetROM(preset_roms::PresetROM& rom_data);

// Loads ROM's box art. This function must be called on the IO thread.
[[nodiscard]] kiwi::nes::Bytes LoadPresetROMBoxArt(
    const preset_roms::PresetROM& rom_data);

struct LoadedPresetROM {
  LoadedPresetROM();
  ~LoadedPresetROM();
  LoadedPresetROM(LoadedPresetROM&&) noexcept;
  LoadedPresetROM& operator=(LoadedPresetROM&&) noexcept;

  LoadedPresetROM(const LoadedPresetROM&) = delete;
  LoadedPresetROM& operator=(const LoadedPresetROM&) = delete;

  kiwi::nes::Bytes rom_data;
  std::unique_ptr<TextureRenderer> texture_renderer;
};

// Loads the playable ROM and, for an HD edition, its texture renderer.
// This function must be called on the IO thread.
[[nodiscard]] LoadedPresetROM LoadPresetROM(
    const preset_roms::PresetROM& rom_data,
    preset_roms::ROMEdition edition);

#endif  // UTILITY_ZIP_READER_H_
