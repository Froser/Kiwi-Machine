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

#ifndef NES_PPU_PATCH_H_
#define NES_PPU_PATCH_H_

#include <stdint.h>
#include "nes/nes_export.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class PPU;
class DebugPort;
// PPUPatch is used to adjust subtle rules for PPU.
// For example, Kirby's Adventure need a scanline iqr in dot 280 instead of 260,
// but generally irq emits at dot 260.
// This class only uses in PPU, all members are private, PPU is its only friend
// class that can access it.
class NES_EXPORT PPUPatch {
  friend class PPU;
  friend class DebugPort;
  PPUPatch();
  ~PPUPatch();
  void Reset();
  void Set(uint32_t rom_crc);

  // Many games assume IRQ starts at the scanline 280, while according to wiki,
  // it should happen on 260.
  // IRQ on 280 has no side-effects for supported games currently, so if there's
  // any game needs a 260-scanline-IRQ, put its CRC32 to Set() and set the
  // expected IRQ scanline.
  int scanline_irq_dot;

  // Give a changes to adjust data address while in rendering pipeline.
  // Punch-out needs this to adjust its data address, to fetch 0xfe tile to
  // switch its CHR bank!
  void (*data_address_patch)(Address* data_address) = nullptr;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_PPU_PATCH_H_