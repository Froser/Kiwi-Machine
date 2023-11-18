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

#ifndef UTILITY_TOUCH_MANAGER_H_
#define UTILITY_TOUCH_MANAGER_H_

#include <SDL.h>

class WindowBase;
struct FingerMotion {
  int x, y;    // Pixels
  int dx, dy;  // Pixels
};

enum class MovingDirection {
  kHorizontal,
  kVertical,
};

class ExclusiveTouchManager {
 public:
  explicit ExclusiveTouchManager(WindowBase* window);
  ~ExclusiveTouchManager();

 public:
  void Handle(SDL_TouchFingerEvent* event);
  FingerMotion GetMotion() const;

  bool is_finger_down() const { return is_finger_down_; }
  bool IsMoving(int distance_threshold = 20) const;
  bool GetTouchPoint(int* x, int* y, int distance_threshold = 20) const;

  MovingDirection GetMovingDirection() const;

 private:
  WindowBase* window_ = nullptr;
  SDL_TouchID touch_id_ = 0;
  SDL_FingerID finger_id_ = 0;
  bool is_finger_down_ = false;
  bool is_moving_ = false;
  mutable bool moving_started_ = false;
  float finger_x_ = 0, finger_y_ = 0;
  float finger_start_x_ = 0, finger_start_y_ = 0;
  float finger_down_x_ = 0, finger_down_y_ = 0;
  float direction_arc_ = 0;
  mutable MovingDirection moving_direction_ = MovingDirection::kHorizontal;
};

#endif  // UTILITY_TOUCH_MANAGER_H_