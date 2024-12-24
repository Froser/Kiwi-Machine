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

#ifndef UI_WIDGETS_FILTER_WIDGET_H_
#define UI_WIDGETS_FILTER_WIDGET_H_

#include <kiwi_nes.h>

#include "ui/widgets/widget.h"

class MainWindow;
class FilterWidget : public Widget {
 public:
  using FilterCallback =
      kiwi::base::RepeatingCallback<void(const std::string&)>;

  explicit FilterWidget(MainWindow* window_base, FilterCallback callback);
  ~FilterWidget() override;

 public:
  void BeginFilter();
  void EndFilter();
  bool has_begun() { return input_started_; }

 public:
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;
  bool OnMouseMove(SDL_MouseMotionEvent* event) override;
  bool OnMouseWheel(SDL_MouseWheelEvent* event) override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;

 private:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnTextInput(SDL_TextInputEvent* event) override;

 private:
  void StartTextInput();
  void StopTextInput();

 private:
  bool input_started_ = false;
  MainWindow* parent_window_ = nullptr;
  FilterCallback callback_ = kiwi::base::DoNothing();
  std::string filter_contents_;
};

#endif  // UI_WIDGETS_FILTER_WIDGET_H_