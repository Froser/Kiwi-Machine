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

#ifndef NES_ROM_DATA_H_
#define NES_ROM_DATA_H_

#include "nes/nes_export.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
// Nametable Mirroring describes the layout of the NES' 2x2 background nametable
// graphics, usually achieved by mirrored memory.
// See https://www.nesdev.org/wiki/Mirroring#Nametable_Mirroring
enum class NametableMirroring {
  kHorizontal = 0,
  kVertical = 1,
  kFourScreen = 8,
  kOneScreenLower,
  kOneScreenHigher,
};

enum class ConsoleType {
  kNESFC = 0,     // Nintendo Entertainment System/Family Computer
  kNVS,           // Nintendo Vs. System
  kPlaychoice10,  // Nintendo Playchoice 10
  kExtend,        // Extended Console Type
};

class NES_EXPORT RomData {
 public:
  RomData();
  ~RomData();

 public:
  Bytes raw_headers;
  Bytes PRG;
  Bytes CHR;
  Byte mapper;
  Byte submapper;
  NametableMirroring name_table_mirroring;
  ConsoleType console_type;
  bool has_extended_ram;
  bool is_nes_20;
  int crc;
};

}  // namespace core
}  // namespace kiwi

#endif  // NES_ROM_DATA_H_
