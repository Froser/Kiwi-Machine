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

#include "nes/ppu_patch.h"

namespace kiwi {
namespace nes {

namespace {
void PUNCH_OUT_data_address_patch(Address* data_address) {
  // Punch-out uses Mapper9.
  // While during the game, in the first scanline and the first dot, tile 0xfe
  // must be fetched to switch its second CHR bank to bank 1, because it
  // contains background's pattern. In demonstration, it works correctly. A
  // scroll will be set to 0xaf(175) and tile 0xfe will be exactly read. But in
  // real game, the scroll set to PPUSCROLL is 0xb0(176), and it makes data
  // address in first scanline become 0x416 instead of 0x415, since the tile
  // 0xfe won't be read.
  // In this scenario, change data address in a hacky way, to make mapper works
  // correctly.
  if (*data_address == 0x416)
    *data_address = 0x415;
}

}  // namespace

PPUPatch::PPUPatch() {
  Reset();
}

PPUPatch::~PPUPatch() = default;

void PPUPatch::Reset() {
  scanline_irq_dot = 280;
  data_address_patch = nullptr;
}

void PPUPatch::Set(uint32_t rom_crc) {
  switch (rom_crc) {
    case 0x3a4d4d10: // Mike Tyson's Punch-Out!! (Europe)
    case 0x92a2185c: // Mike Tyson's Punch-Out!! (USA)
    case 0x25551f3f: // Mike Tyson's Punch-Out!! (Europe) (Rev A)
    case 0x2c818014: // Mike Tyson's Punch-Out!! (Japan, USA) (Rev A)
    case 0xb95e9e7f: // Punch-Out!! (USA)
    case 0x84382231: // Punch-Out!! (Japan) (Gold Edition)
    case 0xd229fd5c: // Punch-Out!! (Europe)
      Reset();
      data_address_patch = PUNCH_OUT_data_address_patch;
      break;
    default:
      Reset();
      break;
  }
}

}  // namespace nes
}  // namespace kiwi