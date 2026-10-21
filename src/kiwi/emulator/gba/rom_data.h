// Copyright (C) 2026 Yisi Yu
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

#ifndef GBA_ROM_DATA_H_
#define GBA_ROM_DATA_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "emulator/gba/gba_export.h"

namespace kiwi {
namespace gba {

enum class SaveType {
  kAuto,
  kNone,
  kSRAM,
  kSRAM512,
  kFlash512,
  kFlash1M,
  kEEPROM,
  kEEPROM512,
};

class GBA_EXPORT RomData {
 public:
  RomData();
  ~RomData();

  std::string title;
  std::string game_code;
  size_t rom_size = 0;
  uint32_t crc32 = 0;
  SaveType save_type = SaveType::kAuto;
  size_t save_size = 0;
};

}  // namespace gba
}  // namespace kiwi

#endif  // GBA_ROM_DATA_H_
