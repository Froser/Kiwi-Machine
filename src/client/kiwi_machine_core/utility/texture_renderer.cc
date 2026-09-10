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

#include "utility/texture_renderer.h"

#include <cstdint>
#include <limits>

#include "nes/ppu_observer.h"

TextureRenderer::TextureRenderer() = default;

TextureRenderer::~TextureRenderer() = default;

bool TextureRenderer::RenderFrame(const kiwi::nes::PPUFrameData& frame,
                                  kiwi::nes::Colors* output) const {
  if (!output || frame.width <= 0 || frame.height <= 0) {
    return false;
  }

  const uint64_t output_width = static_cast<uint64_t>(frame.width) * GetScale();
  const uint64_t output_height =
      static_cast<uint64_t>(frame.height) * GetScale();
  if (output_width > std::numeric_limits<int>::max() ||
      output_height > std::numeric_limits<int>::max() ||
      output_width * output_height > std::numeric_limits<size_t>::max()) {
    return false;
  }

  output->resize(static_cast<size_t>(output_width * output_height));
  const TextureRenderTarget target = {
      output->data(),
      static_cast<size_t>(output_width),
      static_cast<int>(output_width),
      static_cast<int>(output_height),
  };
  return RenderFrame(frame, target);
}
