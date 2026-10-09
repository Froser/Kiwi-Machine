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

#include <array>
#include <cstdlib>

#include "ui/application.h"
#include "ui/widgets/filter_widget.h"
#include "utility/key_mapping_util.h"

namespace {

constexpr int kNavigationInitialRepeatDelayMs = 500;
constexpr int kNavigationRepeatIntervalMs = 100;
constexpr int kNavigationAxisPressThreshold = SDL_JOYSTICK_AXIS_MAX / 3;
constexpr int kNavigationAxisReleaseThreshold = SDL_JOYSTICK_AXIS_MAX / 4;

int ResolveNavigationAxisDirection(int current_direction, Sint16 value) {
  if (current_direction < 0 && value < -kNavigationAxisReleaseThreshold)
    return -1;
  if (current_direction > 0 && value > kNavigationAxisReleaseThreshold)
    return 1;
  if (value <= -kNavigationAxisPressThreshold)
    return -1;
  if (value >= kNavigationAxisPressThreshold)
    return 1;
  return 0;
}

}  // namespace

FlexItemsWidget::Direction FlexItemsWidget::GetSwitchNavigationDirection() {
  std::array<bool, 4> dpad_pressed = {};
  Sint16 strongest_x = 0;
  Sint16 strongest_y = 0;

  for (SDL_GameController* controller :
       Application::Get()->game_controllers()) {
    if (!IsGameControllerInputSupported(controller))
      continue;

    dpad_pressed[kUp] |=
        SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
    dpad_pressed[kDown] |= SDL_GameControllerGetButton(
        controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
    dpad_pressed[kLeft] |= SDL_GameControllerGetButton(
        controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
    dpad_pressed[kRight] |= SDL_GameControllerGetButton(
        controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

    const Sint16 x =
        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
    const Sint16 y =
        SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
    if (std::abs(static_cast<int>(x)) >
        std::abs(static_cast<int>(strongest_x))) {
      strongest_x = x;
    }
    if (std::abs(static_cast<int>(y)) >
        std::abs(static_cast<int>(strongest_y))) {
      strongest_y = y;
    }
  }

  switch_axis_direction_[0] =
      ResolveNavigationAxisDirection(switch_axis_direction_[0], strongest_x);
  switch_axis_direction_[1] =
      ResolveNavigationAxisDirection(switch_axis_direction_[1], strongest_y);

  if (switch_navigation_direction_ != kNone &&
      dpad_pressed[switch_navigation_direction_]) {
    return switch_navigation_direction_;
  }
  if (dpad_pressed[kLeft])
    return kLeft;
  if (dpad_pressed[kRight])
    return kRight;
  if (dpad_pressed[kUp])
    return kUp;
  if (dpad_pressed[kDown])
    return kDown;

  const Direction horizontal =
      switch_axis_direction_[0] < 0
          ? kLeft
          : (switch_axis_direction_[0] > 0 ? kRight : kNone);
  const Direction vertical =
      switch_axis_direction_[1] < 0
          ? kUp
          : (switch_axis_direction_[1] > 0 ? kDown : kNone);
  if (horizontal != kNone && vertical != kNone) {
    if (switch_navigation_direction_ == horizontal ||
        switch_navigation_direction_ == vertical) {
      return switch_navigation_direction_;
    }
    return std::abs(static_cast<int>(strongest_x)) >=
                   std::abs(static_cast<int>(strongest_y))
               ? horizontal
               : vertical;
  }
  return horizontal != kNone ? horizontal : vertical;
}

void FlexItemsWidget::UpdateSwitchNavigation() {
  if (!activate_ || items_.empty() || filter_widget_->has_begun()) {
    ResetSwitchNavigation();
    return;
  }

  const Direction direction = GetSwitchNavigationDirection();
  if (direction == kNone) {
    ResetSwitchNavigation();
    return;
  }

  if (direction != switch_navigation_direction_) {
    switch_navigation_direction_ = direction;
    switch_navigation_repeating_ = false;
    switch_navigation_repeat_timer_.Reset();
    MoveSelection(direction);
    return;
  }

  const int repeat_delay = switch_navigation_repeating_
                               ? kNavigationRepeatIntervalMs
                               : kNavigationInitialRepeatDelayMs;
  if (switch_navigation_repeat_timer_.ElapsedInMilliseconds() < repeat_delay)
    return;

  switch_navigation_repeating_ = true;
  switch_navigation_repeat_timer_.Reset();
  MoveSelection(direction);
}

void FlexItemsWidget::ResetSwitchNavigation() {
  switch_navigation_direction_ = kNone;
  switch_axis_direction_.fill(0);
  switch_navigation_repeating_ = false;
  switch_navigation_repeat_timer_.Reset();
}
