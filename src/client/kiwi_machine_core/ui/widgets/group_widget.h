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

#ifndef UI_WIDGETS_GROUP_WIDGET_H_
#define UI_WIDGETS_GROUP_WIDGET_H_

#include <kiwi_nes.h>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

class MainWindow;
class GroupWidget : public Widget {
 public:
  explicit GroupWidget(MainWindow* main_window, NESRuntimeID runtime_id);
  ~GroupWidget() override;

 public:
  void SetCurrent(int index);
  void RecalculateBounds();

 private:
  void FirstFrame();
  void CalculateItemsBounds(std::vector<SDL_Rect>& container);
  void Layout();
  void ApplyItemBounds();
  void ApplyItemBoundsByFinger();
  int GetNearestIndexByFinger();
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  void IndexChanged();

 protected:
  // Widget:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  std::vector<SDL_Rect> bounds_current_;
  std::vector<SDL_Rect> bounds_next_;
  bool first_paint_ = true;
  int current_idx_ = 0;
  float animation_lerp_ = 0.f;
  Timer animation_counter_;
};

#endif  // UI_WIDGETS_GROUP_WIDGET_H_