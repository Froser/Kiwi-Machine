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

#ifndef UI_WIDGETS_JOYSTICK_BUTTON_H_
#define UI_WIDGETS_JOYSTICK_BUTTON_H_

#include <kiwi_nes.h>

#include "ui/widgets/touch_button.h"

class JoystickButton : public TouchButton {
 public:
  explicit JoystickButton(WindowBase* window_base,
                          image_resources::ImageID image_id);
  ~JoystickButton() override;

 protected:
  // Widget:
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;

 private:
  bool HandleTouchFingerMoveOrDown(SDL_TouchFingerEvent* event);
};

#endif  // UI_WIDGETS_TOUCH_BUTTON_H_