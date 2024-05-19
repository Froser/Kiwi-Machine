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
  // Many games assume IRQ starts at the scanline 280, while according to wiki,
  // it should happen on 260.
  // IRQ on 280 has no side-effects for supported games currently, so if there's
  // any game needs a 260-scanline-IRQ, put its CRC32 to Set() and set the
  // expected IRQ scanline.
  scanline_irq_dot = 280;
}

void PPUPatch::Set(uint32_t rom_crc) {
  switch (rom_crc) {
    default:
      Reset();
      break;
  }
}

}  // namespace nes
}  // namespace kiwi