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
class PresetROM;
}

void FillRomDataFromZip(const preset_roms::PresetROM& rom_data);

#endif  // UTILITY_ZIP_READER_H_