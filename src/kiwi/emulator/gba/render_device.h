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

#ifndef GBA_RENDER_DEVICE_H_
#define GBA_RENDER_DEVICE_H_

#include <cstdint>

#include "emulator/render_device.h"
#include "emulator/gba/gba_export.h"
#include "emulator/gba/types.h"

namespace kiwi {
namespace gba {

struct FrameData {
  int width = kFrameWidth;
  int height = kFrameHeight;
  uint64_t frame_number = 0;
  const Colors* pixels = nullptr;
};

class GBA_EXPORT RenderDevice : public emu::RenderDevice {
 public:
  RenderDevice();
  ~RenderDevice() override;

  virtual void Render(const FrameData& frame) = 0;
};

}  // namespace gba
}  // namespace kiwi

#endif  // GBA_RENDER_DEVICE_H_
