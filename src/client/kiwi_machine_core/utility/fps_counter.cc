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

#include "utility/fps_counter.h"

FpsCounter::FpsCounter() {
  Application::Get()->AddObserver(this);
}

FpsCounter::~FpsCounter() {
  Application::Get()->RemoveObserver(this);
}

void FpsCounter::OnPreRender(int since_last_frame_ms) {
  Update(1000.f / since_last_frame_ms);
}

void FpsCounter::Update(float value) {
  current_fps_ = value;
}

float FpsCounter::GetCurrentFPS() const {
  return current_fps_ > 0.0f ? current_fps_ : 0.0f;
}