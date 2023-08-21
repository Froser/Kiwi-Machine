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

NESFrame::NESFrame(NESRuntimeID runtime_id) : runtime_id_(runtime_id) {}

NESFrame::~NESFrame() = default;

void NESFrame::AddObserver(NESFrameObserver* observer) {
  observers_.insert(observer);
}

void NESFrame::RemoveObserver(NESFrameObserver* observer) {
  observers_.erase(observer);
}

void NESFrame::Render(int width, int height, const Buffer& buffer) {
  buffer_ = buffer;
  render_width_ = width;
  render_height_ = height;

  int elapsed_ms = frame_elapsed_counter_.ElapsedInMillisecondsAndReset();
  for (NESFrameObserver* observer : observers_) {
    observer->OnShouldRender(elapsed_ms);
  }
}

bool NESFrame::NeedRender() {
  return true;
}
