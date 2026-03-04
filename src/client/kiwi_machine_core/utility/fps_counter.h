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

#ifndef UTILITY_FPS_COUNTER_H_
#define UTILITY_FPS_COUNTER_H_

#include <kiwi_nes.h>

#include "ui/application.h"

class FpsCounter : public ApplicationObserver {
 public:
  FpsCounter();
  ~FpsCounter() override;

  float GetCurrentFPS() const;

 protected:
  void OnPreRender(int since_last_frame_ms) override;

 private:
  void Update(float value);

 private:
  float current_fps_ = 0.0f;
};

#endif  // UTILITY_FPS_COUNTER_H_