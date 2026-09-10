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

#ifndef UTILITY_TEXTURE_RENDERER_H_
#define UTILITY_TEXTURE_RENDERER_H_

#include <cstddef>
#include <cstdint>

#include "nes/types.h"

namespace kiwi {
namespace nes {
struct PPUFrameData;
}  // namespace nes
}  // namespace kiwi

struct TextureRenderTarget {
  kiwi::nes::Color* pixels = nullptr;
  // Number of Color elements between adjacent rows.
  size_t stride = 0;
  int width = 0;
  int height = 0;
};

// Converts format-neutral PPU tile commands into a presentation frame.
class TextureRenderer {
 public:
  TextureRenderer();
  virtual ~TextureRenderer();

  TextureRenderer(const TextureRenderer&) = delete;
  TextureRenderer& operator=(const TextureRenderer&) = delete;

  virtual uint32_t GetScale() const = 0;
  virtual bool IsUseChrRam() const = 0;
  bool RenderFrame(const kiwi::nes::PPUFrameData& frame,
                   kiwi::nes::Colors* output) const;
  virtual bool RenderFrame(const kiwi::nes::PPUFrameData& frame,
                           const TextureRenderTarget& target) const = 0;
};

#endif  // UTILITY_TEXTURE_RENDERER_H_
