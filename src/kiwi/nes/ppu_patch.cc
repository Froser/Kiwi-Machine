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

PPUPatch::PPUPatch() {
  Reset();
}
PPUPatch::~PPUPatch() = default;

void PPUPatch::Reset() {
  scanline_irq_dot = 260;
}

void PPUPatch::Set(uint32_t rom_crc) {
  switch (rom_crc) {
    case 0x37088EFF:  // Kirby's Adventure	Canada
    case 0xD7794AFC:  // Kirby's Adventure	USA
    case 0xB2EF7F4B:  // Kirby's Adventure	France
    case 0x127D76F4:  // Kirby's Adventure	Germany
    case 0x2C088DC5:  // Kirby's Adventure	Scandinavia
    case 0x5ED6F221:  // Kirby's Adventure	USA (Rev A)
      scanline_irq_dot = 280;
      break;
    default:
      Reset();
      break;
  }
}

}  // namespace nes
}  // namespace kiwi