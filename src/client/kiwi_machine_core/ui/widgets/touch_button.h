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

#ifndef UI_WIDGETS_TOUCH_BUTTON_H_
#define UI_WIDGETS_TOUCH_BUTTON_H_

#include <kiwi_nes.h>
#include <map>
#include <string>

#include "resources/image_resources.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

// A demo widget shows IMGui's demo.
class TouchButton : public Widget {
 public:
  explicit TouchButton(WindowBase* window_base,
                       image_resources::ImageID image_id);
  ~TouchButton() override;

  void set_trigger_callback(const kiwi::base::RepeatingClosure& callback) {
    trigger_callback_ = callback;
  }

 protected:
  // Widget:
  void Paint() override;
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;

 private:
  void CalculateButtonState();

 private:
  enum class ButtonState {
    kNormal,
    kDown,
  };

  struct TouchDetail {
    int touch_point_x;
    int touch_point_y;
  };

  kiwi::base::RepeatingClosure trigger_callback_;
  bool first_paint_ = true;
  SDL_Texture* texture_ = nullptr;
  image_resources::ImageID image_id_;
  std::map<int, TouchDetail> triggered_fingers_;
  ButtonState button_state_ = ButtonState::kNormal;
};

#endif  // UI_WIDGETS_TOUCH_BUTTON_H_