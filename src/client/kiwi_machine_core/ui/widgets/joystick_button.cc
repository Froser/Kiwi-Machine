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

#include "ui/widgets/joystick_button.h"

#include <imgui.h>

#include "ui/window_base.h"
#include "utility/images.h"
#include "utility/math.h"

JoystickButton::JoystickButton(WindowBase* window_base,
                               image_resources::ImageID image_id)
    : TouchButton(window_base, image_id) {}

JoystickButton::~JoystickButton() = default;

bool JoystickButton::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  return HandleTouchFingerMoveOrDown(event);
}

bool JoystickButton::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  if (triggered_fingers_.erase(event->fingerId)) {
    SDL_Rect bounds = window()->GetClientBounds();
    ImVec2 touch_pt(event->x * bounds.w, event->y * bounds.h);
    if (Contains(MapToWindow(this->bounds()), touch_pt.x, touch_pt.y) &&
        trigger_callback_) {
      button_state_ = ButtonState::kNormal;
      trigger_callback_.Run();
    }
  }

  return false;
}

bool JoystickButton::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  return HandleTouchFingerMoveOrDown(event);
}

bool JoystickButton::HandleTouchFingerMoveOrDown(SDL_TouchFingerEvent* event) {
  SDL_Rect bounds = window()->GetClientBounds();
  ImVec2 touch_pt(event->x * bounds.w, event->y * bounds.h);
  if (Contains(MapToWindow(this->bounds()), touch_pt.x, touch_pt.y)) {
    auto finger_iter = triggered_fingers_.find(event->fingerId);
    if (finger_iter == triggered_fingers_.end()) {
      triggered_fingers_.insert(std::make_pair(
          event->fingerId, TouchDetail{static_cast<int>(touch_pt.x),
                                       static_cast<int>(touch_pt.y)}));
      if (finger_down_callback_)
        finger_down_callback_.Run();
    }
    button_state_ = ButtonState::kDown;
  } else {
    auto finger_iter = triggered_fingers_.find(event->fingerId);
    if (finger_iter != triggered_fingers_.end() && trigger_callback_) {
      triggered_fingers_.erase(event->fingerId);
      button_state_ = ButtonState::kNormal;
      trigger_callback_.Run();
    }
  }

  return false;
}