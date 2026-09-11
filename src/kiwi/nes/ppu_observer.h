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

#ifndef NES_PPU_OBSERVER_H_
#define NES_PPU_OBSERVER_H_

#include <span>

#include "nes/ppu_texture_capture.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

struct PPUFrameData {
  enum class Type {
    kNativePixels,
    kTextureMetadata,
  };

  // Selects which rendering payload consumers should use.
  Type type = Type::kNativePixels;
  // Frame dimensions in native PPU pixels.
  int width = 256;
  int height = 240;
  // Original PPU output, valid for the duration of OnRenderReady().
  const Colors* native_pixels = nullptr;
  // The borrowed metadata spans below are valid only during OnRenderReady().
  // Per-pixel backdrop colors before background and sprite composition.
  std::span<const Color> texture_backdrop_pixels;
  // Background tile commands in PPU rasterization order.
  std::span<const PPUTextureTileCommand> texture_background_tiles;
  // Sprite tile commands with their original OAM ordering information.
  std::span<const PPUTextureTileCommand> texture_sprite_tiles;
  // Snapshot of $3F00-$3F1F used by Mesen PPU memory conditions.
  std::span<const Byte> texture_ppu_palette;
};

class PPUObserver {
 public:
  PPUObserver();
  virtual ~PPUObserver();

  virtual void OnPPUStepped() {}
  virtual void OnPPUADDR(Address address) {}
  virtual void OnPPUScanlineStart(int scanline) {}
  virtual void OnPPUScanlineEnd(int scanline) {}
  virtual void OnPPUFrameStart() {}
  virtual void OnPPUFrameEnd() {}
  // If all visible scanlines are rendered, this method will be called.
  virtual void OnRenderReady(const PPUFrameData& frame) {}
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_PPU_OBSERVER_H_
