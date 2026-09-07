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

#include "ui/widgets/virtual_joystick.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

#include "ui/window_base.h"
#include "utility/math.h"

namespace {
constexpr ImU32 kPadFillColor = IM_COL32(24, 29, 27, 150);
constexpr ImU32 kPadRingColor = IM_COL32(143, 153, 147, 190);
constexpr ImU32 kPadGuideColor = IM_COL32(143, 153, 147, 70);
constexpr ImU32 kPadActiveColor = IM_COL32(101, 216, 75, 220);
constexpr ImU32 kThumbFillColor = IM_COL32(68, 76, 71, 235);
constexpr ImU32 kThumbRingColor = IM_COL32(224, 230, 226, 220);
}  // namespace

VirtualJoystick::VirtualJoystick(WindowBase* window_base)
    : Widget(window_base) {
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBackground;
  set_flags(window_flags);
  set_title("##VirtualJoystick");
}

VirtualJoystick::~VirtualJoystick() = default;

void VirtualJoystick::Paint() {
  const float pad_radius = pad_scaling_ * bounds().w / 2.f;
  const float ball_radius = ball_scaling_ * bounds().w / 2.f;
  const ImVec2 pad_center(bounds().x + bounds().w / 2.f,
                          bounds().y + bounds().h / 2.f);
  ImVec2 ball_center = pad_center;

  if (is_finger_down_) {
    SDL_Rect bounds = window()->GetClientBounds();
    SDL_Rect finger_pos{static_cast<int>(finger_x_ * bounds.w),
                        static_cast<int>(finger_y_ * bounds.h)};

    int distance2_to_center =
        (finger_pos.x - pad_center.x) * (finger_pos.x - pad_center.x) +
        (finger_pos.y - pad_center.y) * (finger_pos.y - pad_center.y);

    if (distance2_to_center > (this->bounds().w / 2 * fixed_threshold_) *
                                  (this->bounds().w / 2 * fixed_threshold_)) {
      if (distance2_to_center < pad_radius * pad_radius) {
        ball_center.x = finger_pos.x;
        ball_center.y = finger_pos.y;
      } else {
        float distance_to_center = std::sqrt(distance2_to_center);
        float sin = (finger_pos.y - pad_center.y) / distance_to_center;
        float cos = (finger_pos.x - pad_center.x) / distance_to_center;
        ball_center.y = pad_center.y + sin * pad_radius;
        ball_center.x = pad_center.x + cos * pad_radius;
      }
    }
  }

  ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
  draw_list->AddCircleFilled(pad_center, pad_radius, kPadFillColor, 64);
  draw_list->AddCircle(pad_center, pad_radius, kPadRingColor, 64,
                       std::max(2.f, bounds().w * .012f));
  draw_list->AddCircle(pad_center, pad_radius * .72f, kPadGuideColor, 64,
                       std::max(1.f, bounds().w * .006f));
  if (is_finger_down_) {
    draw_list->AddLine(pad_center, ball_center, kPadActiveColor,
                       std::max(2.f, bounds().w * .018f));
  }
  draw_list->AddCircleFilled(ball_center, ball_radius, kThumbFillColor, 48);
  draw_list->AddCircle(ball_center, ball_radius,
                       is_finger_down_ ? kPadActiveColor : kThumbRingColor, 48,
                       std::max(2.f, bounds().w * .012f));
}

bool VirtualJoystick::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  if (!is_finger_down_) {
    // If the finger is too far away from the center of the virtual joystick, do
    // not handle the event.
    ImVec2 pad_center(bounds().x + (bounds().w / 2),
                      bounds().y + (bounds().h / 2));
    SDL_Rect bounds = window()->GetClientBounds();
    SDL_Rect finger_pos{static_cast<int>(event->x * bounds.w),
                        static_cast<int>(event->y * bounds.h)};
    int distance2_to_center =
        (finger_pos.x - pad_center.x) * (finger_pos.x - pad_center.x) +
        (finger_pos.y - pad_center.y) * (finger_pos.y - pad_center.y);
    if (distance2_to_center > (ignore_threshold_ * this->bounds().w / 2) *
                                  (ignore_threshold_ * this->bounds().w / 2))
      return false;

    is_finger_down_ = true;
    finger_id_ = event->fingerId;
    finger_x_ = event->x;
    finger_y_ = event->y;

    CalculateJoystick();
  }

  return false;
}

bool VirtualJoystick::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  if (event->fingerId == finger_id_) {
    is_finger_down_ = false;

    CalculateJoystick();
  }
  return false;
}

bool VirtualJoystick::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  if (is_finger_down_ && event->fingerId == finger_id_) {
    finger_x_ = event->x;
    finger_y_ = event->y;

    CalculateJoystick();
  }
  return false;
}

int VirtualJoystick::GetHitTestPolicy() {
  return Widget::GetHitTestPolicy() | kAlwaysHitTest;
}

void VirtualJoystick::CalculateJoystick() {
  int joystick_states = kNotPressed;
  if (is_finger_down_) {
    ImVec2 pad_center(bounds().x + (bounds().w / 2),
                      bounds().y + (bounds().h / 2));
    SDL_Rect bounds = window()->GetClientBounds();
    SDL_Rect finger_pos{static_cast<int>(finger_x_ * bounds.w),
                        static_cast<int>(finger_y_ * bounds.h)};
    int distance2_to_center =
        (finger_pos.x - pad_center.x) * (finger_pos.x - pad_center.x) +
        (finger_pos.y - pad_center.y) * (finger_pos.y - pad_center.y);

    if (distance2_to_center > (this->bounds().w / 2 * fixed_threshold_) *
                                  (this->bounds().w / 2 * fixed_threshold_)) {
      float distance_to_center = std::sqrt(distance2_to_center);

      // Screen coordinate is upside-down.
      float sin = -(finger_pos.y - pad_center.y) / distance_to_center;

      bool is_right_quadrant = (finger_pos.x - pad_center.x) > 0;
      float rad = std::asin(sin);

      if (-M_PI / 2 <= rad && rad < -3 * M_PI / 8) {
        joystick_states |= kDown;
      } else if (-3 * M_PI / 8 <= rad && rad < -M_PI / 8) {
        joystick_states |= kDown;
        joystick_states |= is_right_quadrant ? kRight : kLeft;
      } else if (-M_PI / 8 <= rad && rad < M_PI / 8) {
        joystick_states |= is_right_quadrant ? kRight : kLeft;
      } else if (M_PI / 8 <= rad && rad < 3 * M_PI / 8) {
        joystick_states |= kUp;
        joystick_states |= is_right_quadrant ? kRight : kLeft;
      } else {
        joystick_states |= kUp;
      }
    }
  }

  if (callback_)
    callback_.Run(joystick_states);
}
