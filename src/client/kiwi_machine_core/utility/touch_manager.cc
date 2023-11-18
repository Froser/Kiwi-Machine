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

#include "utility/touch_manager.h"

#include "ui/window_base.h"

ExclusiveTouchManager::ExclusiveTouchManager(WindowBase* window)
    : window_(window) {}
ExclusiveTouchManager::~ExclusiveTouchManager() = default;

void ExclusiveTouchManager::Handle(SDL_TouchFingerEvent* event) {
  if (event->type == SDL_FINGERDOWN) {
    if (!is_finger_down_) {
      is_finger_down_ = true;
      touch_id_ = event->touchId;
      finger_id_ = event->fingerId;
      finger_start_x_ = finger_x_ = event->x;
      finger_start_y_ = finger_y_ = event->y;
      finger_down_x_ = finger_start_x_;
      finger_down_y_ = finger_start_y_;
      moving_started_ = is_moving_ = false;
    }
  } else if (event->type == SDL_FINGERUP) {
    if (touch_id_ == event->touchId && finger_id_ == event->fingerId) {
      is_finger_down_ = false;
      touch_id_ = finger_id_ = 0;
      finger_start_x_ = finger_start_y_ = finger_x_ = finger_y_ = 0;
      moving_started_ = is_moving_ = false;
    }
  } else if (event->type == SDL_FINGERMOTION) {
    if (touch_id_ == event->touchId && finger_id_ == event->fingerId) {
      finger_x_ = event->x;
      finger_y_ = event->y;
      is_moving_ = true;
    }
  }
}

FingerMotion ExclusiveTouchManager::GetMotion() const {
  SDL_assert(is_finger_down());
  SDL_Rect bounds = window_->GetClientBounds();
  return FingerMotion{
      static_cast<int>(finger_start_x_ * bounds.w),
      static_cast<int>(finger_start_y_ * bounds.h),
      static_cast<int>((finger_x_ - finger_start_x_) * bounds.w),
      static_cast<int>((finger_y_ - finger_start_y_) * bounds.h)};
}

bool ExclusiveTouchManager::IsMoving(int distance_threshold) const {
  if (moving_started_)
    return true;

  if (is_moving_) {
    FingerMotion motion = GetMotion();
    moving_started_ = motion.dx * motion.dx + motion.dy * motion.dy >
                      distance_threshold * distance_threshold;
    moving_direction_ = (std::abs(motion.dx) >= std::abs(motion.dy))
                            ? MovingDirection::kHorizontal
                            : MovingDirection::kVertical;
    return moving_started_;
  }

  return false;
}

bool ExclusiveTouchManager::GetTouchPoint(int* x,
                                          int* y,
                                          int distance_threshold) const {
  SDL_assert(is_finger_down());
  if (IsMoving(distance_threshold))
    return false;

  FingerMotion motion = GetMotion();
  float moving_distance_2 = motion.dx * motion.dx + motion.dy * motion.dy;
  if (moving_distance_2 > distance_threshold * distance_threshold)
    return false;

  SDL_Rect bounds = window_->GetClientBounds();
  *x = finger_down_x_ * bounds.w;
  *y = finger_down_y_ * bounds.h;
  return true;
}

MovingDirection ExclusiveTouchManager::GetMovingDirection() const {
  SDL_assert(is_moving_ || moving_started_);
  return moving_direction_;
}