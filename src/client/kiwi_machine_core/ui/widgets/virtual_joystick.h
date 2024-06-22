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

#ifndef UI_WIDGETS_VIRTUAL_JOYSTICK_H_
#define UI_WIDGETS_VIRTUAL_JOYSTICK_H_

#include <kiwi_nes.h>
#include <map>
#include <string>

#include "resources/image_resources.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

class VirtualJoystick : public Widget {
 public:
  enum State {
    kNotPressed = 0,
    kLeft = 1,
    kRight = 2,
    kUp = 4,
    kDown = 8,
  };

  // See type 'State'.
  using JoystickCallback = kiwi::base::RepeatingCallback<void(int)>;

  explicit VirtualJoystick(WindowBase* window_base);
  ~VirtualJoystick() override;

  void set_joystick_callback(JoystickCallback callback) {
    callback_ = callback;
  }

 protected:
  // Widget:
  void Paint() override;
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;
  int GetHitTestPolicy() override;

 private:
  void CalculateJoystick();

 private:
  bool first_paint_ = true;
  float pad_scaling_ = .8f;
  float ball_scaling_ = .3f;
  float fixed_threshold_ = .2f;
  float ignore_threshold_ = 1.4f;
  SDL_Texture* texture_pad_ = nullptr;
  SDL_Texture* texture_ball_ = nullptr;
  image_resources::ImageID image_id_;
  float finger_x_ = 0;
  float finger_y_ = 0;
  SDL_FingerID finger_id_ = 0;
  bool is_finger_down_ = false;
  JoystickCallback callback_;
};

#endif  // UI_WIDGETS_VIRTUAL_JOYSTICK_H_