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

#ifndef PRESET_ROMS_DEFINES_H
#define PRESET_ROMS_DEFINES_H
#include <kiwi_nes.h>

namespace preset_roms {
struct PresetROM {
  const char* name;
  const kiwi::nes::Byte* zip_data;
  size_t zip_size;

  // Uncompressed data. Filled by FillRomDataFromZip().
  mutable kiwi::nes::Bytes rom_data;
  mutable kiwi::nes::Bytes rom_cover;

  // Switch ROM version.
  mutable std::vector<PresetROM> alternates;
};

#define PRESET_ROM(name) name::ROM_NAME, name::ROM_ZIP, name::ROM_ZIP_SIZE

#define EXTERN_ROM(name)                  \
  namespace name {                        \
  extern const char ROM_NAME[];           \
  extern const kiwi::nes::Byte ROM_ZIP[]; \
  extern size_t ROM_ZIP_SIZE;             \
  }

#include "preset_roms/preset_roms.inc"

}  // namespace preset_roms

#undef EXTERN_ROM
#endif  // PRESET_ROMS_DEFINES_H
