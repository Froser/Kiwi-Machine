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
#include <cmath>

#include "ui/window_base.h"
#include "utility/images.h"
#include "utility/math.h"

VirtualJoystick::VirtualJoystick(WindowBase* window_base)
    : Widget(window_base) {
  SDL_assert(image_id_ != image_resources::ImageID::kLast);
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
  if (first_paint_) {
    SDL_assert(!texture_pad_);
    texture_pad_ = GetImage(window()->renderer(),
                            image_resources::ImageID::kVtbJoystickPad);

    SDL_assert(!texture_ball_);
    texture_ball_ = GetImage(window()->renderer(),
                             image_resources::ImageID::kVtbJoystickBall);

    first_paint_ = false;
  }

  int pad_radius = pad_scaling_ * bounds().w / 2;
  int ball_radius = ball_scaling_ * bounds().w / 2;
  ImVec2 pad_center(bounds().x + (bounds().w / 2),
                    bounds().y + (bounds().h / 2));

  SDL_Rect ball_rect = {
      static_cast<int>(pad_center.x - ball_radius),
      static_cast<int>(pad_center.y - ball_radius),
      ball_radius * 2,
      ball_radius * 2,
  };

  if (is_finger_down_) {
    SDL_Rect bounds = window()->GetClientBounds();
    SDL_Rect finger_pos{static_cast<int>(finger_x_ * bounds.w),
                        static_cast<int>(finger_y_ * bounds.h)};

    int distance2_to_center =
        (finger_pos.x - pad_center.x) * (finger_pos.x - pad_center.x) +
        (finger_pos.y - pad_center.y) * (finger_pos.y - pad_center.y);

    if (distance2_to_center > (this->bounds().w / 2 * fixed_threshold_) *
                                  (this->bounds().w / 2 * fixed_threshold_)) {
      ImVec2 ball_center;
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
      ball_rect = {
          static_cast<int>(ball_center.x - ball_radius),
          static_cast<int>(ball_center.y - ball_radius),
          ball_radius * 2,
          ball_radius * 2,
      };
    }
  }

  SDL_Rect pad_rect{
      bounds().x + (bounds().w / 2) - pad_radius,
      bounds().y + (bounds().h / 2) - pad_radius,
      pad_radius * 2,
      pad_radius * 2,
  };

  // Draw joystick pad
  ImGui::GetBackgroundDrawList()->AddImage(
      reinterpret_cast<ImTextureID>(texture_pad_),
      ImVec2(pad_rect.x, pad_rect.y),
      ImVec2(pad_rect.x + pad_rect.w, pad_rect.y + pad_rect.h));

  // Draw joystick ball
  ImGui::GetBackgroundDrawList()->AddImage(
      reinterpret_cast<ImTextureID>(texture_ball_),
      ImVec2(ball_rect.x, ball_rect.y),
      ImVec2(ball_rect.x + ball_rect.w, ball_rect.y + ball_rect.h));
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