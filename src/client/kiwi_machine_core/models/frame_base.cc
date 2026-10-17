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

#include "models/frame_base.h"

#include "ui/window_base.h"

FrameBase::FrameBase(WindowBase* window, Uint32 pixel_format)
    : window_(window), pixel_format_(pixel_format) {
  SDL_assert(window_);
}

FrameBase::~FrameBase() {
  if (screen_texture_) {
    SDL_DestroyTexture(screen_texture_);
  }
}

bool FrameBase::EnsureTexture(int width, int height) {
  if (width <= 0 || height <= 0) {
    return false;
  }
  if (screen_texture_ && render_width_ == width && render_height_ == height) {
    return true;
  }

  if (screen_texture_) {
    SDL_DestroyTexture(screen_texture_);
    screen_texture_ = nullptr;
  }
  screen_texture_ =
      SDL_CreateTexture(window_->renderer(), pixel_format_,
                        SDL_TEXTUREACCESS_STREAMING, width, height);
  if (!screen_texture_) {
    render_width_ = 0;
    render_height_ = 0;
    return false;
  }

  render_width_ = width;
  render_height_ = height;
  return true;
}

bool FrameBase::UpdateTexture(int width,
                              int height,
                              const kiwi::emu::Colors& buffer) {
  if (!EnsureTexture(width, height)) {
    return false;
  }

  const int result = SDL_UpdateTexture(screen_texture_, nullptr, buffer.data(),
                                       render_width_ * sizeof(buffer[0]));
  SDL_assert(result == 0);
  return result == 0;
}
