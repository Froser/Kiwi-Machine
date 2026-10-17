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

#ifndef MODELS_FRAME_BASE_H_
#define MODELS_FRAME_BASE_H_

#include <SDL.h>

#include "base/memory/ref_counted.h"
#include "emulator/types.h"

class WindowBase;

class FrameBase : public kiwi::base::RefCounted<FrameBase> {
  friend class kiwi::base::RefCounted<FrameBase>;

 public:
  int width() const { return render_width_; }
  int height() const { return render_height_; }
  SDL_Texture* texture() const { return screen_texture_; }

 protected:
  FrameBase(WindowBase* window, Uint32 pixel_format);
  virtual ~FrameBase();

  bool EnsureTexture(int width, int height);
  bool UpdateTexture(int width, int height, const kiwi::emu::Colors& buffer);

 private:
  WindowBase* const window_;
  const Uint32 pixel_format_;
  SDL_Texture* screen_texture_ = nullptr;
  int render_width_ = 0;
  int render_height_ = 0;
};

#endif  // MODELS_FRAME_BASE_H_
