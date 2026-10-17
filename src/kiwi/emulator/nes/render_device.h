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

#ifndef NES_RENDER_DEVICE_H_
#define NES_RENDER_DEVICE_H_

#include "emulator/render_device.h"
#include "emulator/nes/nes_export.h"

namespace kiwi {
namespace nes {

struct PPUFrameData;

class NES_EXPORT RenderDevice : public emu::RenderDevice {
 public:
  RenderDevice();
  ~RenderDevice() override;

  virtual void Render(const PPUFrameData& frame) = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_RENDER_DEVICE_H_
