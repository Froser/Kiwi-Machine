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

#include "models/nes_frame.h"
#include "ui/window_base.h"

NESFrame::NESFrame(WindowBase* window, NESRuntimeID runtime_id)
    : window_(window) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  SDL_assert(runtime_data_);
}

NESFrame::~NESFrame() {
  if (screen_texture_)
    SDL_DestroyTexture(screen_texture_);
}

void NESFrame::AddObserver(NESFrameObserver* observer) {
  observers_.insert(observer);
}

void NESFrame::RemoveObserver(NESFrameObserver* observer) {
  observers_.erase(observer);
}

void NESFrame::Render(int width, int height, const kiwi::nes::Colors& buffer) {
  // Creates texture if not exists or size changed
  if (render_width_ != width || render_height_ != height) {
    render_width_ = width;
    render_height_ = height;
    if (screen_texture_) {
      SDL_DestroyTexture(screen_texture_);
      screen_texture_ = nullptr;
    }

    if (!screen_texture_) {
      SDL_assert(render_width_ > 0 && render_height_ > 0);
      screen_texture_ = SDL_CreateTexture(
          window_->renderer(), SDL_PIXELFORMAT_ARGB8888,
          SDL_TEXTUREACCESS_STREAMING, render_width_, render_height_);
    }
  }

  // Updates contents
  int result = SDL_UpdateTexture(screen_texture_, nullptr, buffer.data(),
                                 render_width_ * sizeof(buffer[0]));
  SDL_assert(result == 0);

  // Notifies observers
  if (!observers_.empty()) {
    int elapsed_ms = frame_elapsed_counter_.ElapsedInMillisecondsAndReset();
    for (NESFrameObserver* observer : observers_) {
      observer->OnShouldRender(elapsed_ms);
    }
  }
}

bool NESFrame::NeedRender() {
  return true;
}

const NESFrame::Buffer& NESFrame::GetLastFrame() {
  return runtime_data_->emulator->GetLastFrame();
}


const NESFrame::Buffer& NESFrame::GetCurrentFrame() {
  return runtime_data_->emulator->GetCurrentFrame();
}
