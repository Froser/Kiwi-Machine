// Copyright (C) 2024 Yisi Yu
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

#include "ui/widgets/flex_items_widget.h"

#include <SDL.h>

#include <algorithm>

#include "ui/window_base.h"

namespace {

constexpr float kVelocitySmoothing = 0.35f;
constexpr float kMaximumFlingVelocity = 6000.f;
constexpr Uint32 kVelocitySampleTimeoutMs = 100;

}  // namespace

bool FlexItemsWidget::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  if (touch_active_)
    return event->fingerId == active_touch_id_;

  const bool interrupted_inertial_scrolling = inertial_scrolling_;
  StopInertialScrolling();
  touch_active_ = true;
  active_touch_id_ = event->fingerId;
  last_finger_event_ = *event;
  finger_scroll_velocity_ = 0.f;
  finger_scroll_remainder_ = 0.f;
  has_finger_velocity_sample_ = false;
  scrolling_by_finger_ = interrupted_inertial_scrolling;

  SDL_Rect window_rect = window()->GetWindowBounds();
  return HandleMouseOrFingerEvents(
      MouseOrFingerEventType::kFingerDown, MouseButton::kLeftButton,
      event->x * window_rect.w, event->y * window_rect.h);
}

bool FlexItemsWidget::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  if (activate_ && gesture_locked_ && touch_active_ &&
      event->fingerId == active_touch_id_) {
    SDL_Rect window_rect = window()->GetWindowBounds();
    const float precise_y_distance =
        (event->y - last_finger_event_.y) * window_rect.h +
        finger_scroll_remainder_;
    const int y_distance = static_cast<int>(precise_y_distance);
    finger_scroll_remainder_ = precise_y_distance - y_distance;
    if (y_distance != 0) {
      scrolling_by_finger_ = true;
      ScrollWith(y_distance, nullptr, nullptr);
    }

    const Uint32 elapsed_ms = event->timestamp - last_finger_event_.timestamp;
    if (elapsed_ms > 0) {
      const float velocity =
          std::clamp((event->y - last_finger_event_.y) * window_rect.h *
                         1000.f / elapsed_ms,
                     -kMaximumFlingVelocity, kMaximumFlingVelocity);
      if (has_finger_velocity_sample_) {
        finger_scroll_velocity_ +=
            (velocity - finger_scroll_velocity_) * kVelocitySmoothing;
      } else {
        finger_scroll_velocity_ = velocity;
        has_finger_velocity_sample_ = true;
      }
    }

    last_finger_event_ = *event;
  }
  return false;
}

bool FlexItemsWidget::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  if (!touch_active_ || event->fingerId != active_touch_id_)
    return false;

  SDL_Rect window_rect = window()->GetWindowBounds();
  bool handled = HandleMouseOrFingerEvents(
      MouseOrFingerEventType::kFingerUp, MouseButton::kLeftButton,
      event->x * window_rect.w, event->y * window_rect.h);
  if (scrolling_by_finger_ && event->timestamp - last_finger_event_.timestamp <=
                                  kVelocitySampleTimeoutMs) {
    StartInertialScrolling(finger_scroll_velocity_);
  }

  touch_active_ = false;
  scrolling_by_finger_ = false;
  return handled;
}
