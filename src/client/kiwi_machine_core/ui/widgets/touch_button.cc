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

#include "ui/widgets/touch_button.h"

#include <imgui.h>

#include "ui/window_base.h"
#include "utility/images.h"
#include "utility/math.h"

TouchButton::TouchButton(WindowBase* window_base,
                         image_resources::ImageID image_id)
    : Widget(window_base), image_id_(image_id) {
  SDL_assert(image_id_ != image_resources::ImageID::kLast);
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBackground;
  set_flags(window_flags);
  set_title("##TouchButton");
  texture_ = GetImage(window()->renderer(), image_id_);

  SDL_QueryTexture(texture_, nullptr, nullptr, &texture_width_,
                   &texture_height_);
  SDL_Rect b = bounds();
  b.w = texture_width_;
  b.h = texture_height_;
  set_bounds(b);
}

TouchButton::~TouchButton() = default;

void TouchButton::Paint() {
  ImU32 color = button_state_ == ButtonState::kNormal
                    ? IM_COL32(255, 255, 255, opacity_ * 255)
                    : IM_COL32(255, 255, 255, opacity_ * 128);
  SDL_Rect rect = bounds();
  ImGui::GetBackgroundDrawList()->AddImage(
      texture_, ImVec2(rect.x, rect.y),
      ImVec2(rect.x + rect.w, rect.y + rect.h), ImVec2(0, 0), ImVec2(1, 1),
      color);
}

bool TouchButton::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  bool handled = false;
  SDL_Rect bounds = window()->GetClientBounds();
  ImVec2 touch_pt(event->x * bounds.w, event->y * bounds.h);
  if (Contains(MapToWindow(this->bounds()), touch_pt.x, touch_pt.y)) {
    triggered_fingers_.insert(std::make_pair(
        event->fingerId, TouchDetail{static_cast<int>(touch_pt.x),
                                     static_cast<int>(touch_pt.y)}));
    if (finger_down_callback_)
      finger_down_callback_.Run();

    handled = true;
  }

  CalculateButtonState();
  return handled;
}

bool TouchButton::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  bool handled = false;
  if (triggered_fingers_.erase(event->fingerId))
    handled = true;

  ButtonState previous_button_state = button_state_;
  CalculateButtonState();

  // If there's no finger on it (button state equals to kNormal), trigger the
  // button.
  if (previous_button_state == ButtonState::kDown &&
      button_state_ == ButtonState::kNormal && visible() && enabled() &&
      trigger_callback_) {
    trigger_callback_.Run();
  }

  return handled;
}

bool TouchButton::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  bool handled = false;
  auto finger_iter = triggered_fingers_.find(event->fingerId);
  if (finger_iter != triggered_fingers_.end()) {
    SDL_Rect bounds = window()->GetClientBounds();
    finger_iter->second.touch_point_x = event->x * bounds.w;
    finger_iter->second.touch_point_y = event->y * bounds.h;
    handled = true;
  }

  CalculateButtonState();
  return handled;
}

int TouchButton::GetHitTestPolicy() {
  return Widget::GetHitTestPolicy() | kAlwaysHitTest;
}

void TouchButton::CalculateButtonState() {
  button_state_ = ButtonState::kNormal;
  for (const auto& finger : triggered_fingers_) {
    if (Contains(MapToWindow(this->bounds()), finger.second.touch_point_x,
                 finger.second.touch_point_y))
      button_state_ = ButtonState::kDown;
  }
}