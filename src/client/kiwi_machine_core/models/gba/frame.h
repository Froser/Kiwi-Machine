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

#ifndef MODELS_GBA_FRAME_H_
#define MODELS_GBA_FRAME_H_

#include <SDL.h>

#include "emulator/gba/io_devices.h"
#include "models/frame_base.h"

class WindowBase;

class GBAFrame : public kiwi::gba::IODevices::RenderDevice, public FrameBase {
 public:
  static constexpr Uint32 kPixelFormat = SDL_PIXELFORMAT_ABGR8888;

  explicit GBAFrame(WindowBase* window);

  void Render(const kiwi::gba::FrameData& frame) override;
  bool NeedRender() override;

 private:
  ~GBAFrame() override;
};

#endif  // MODELS_GBA_FRAME_H_
