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

#ifndef UI_WIDGETS_LOADING_WIDGET_H_
#define UI_WIDGETS_LOADING_WIDGET_H_

#include "ui/widgets/widget.h"
#include "utility/timer.h"

class MainWindow;
class LoadingWidget : public Widget {
 public:
  explicit LoadingWidget(MainWindow* main_window);
  ~LoadingWidget() override;

  void set_circle_count(int count) { circle_count_ = count; }
  void set_color(SDL_Color color) { color_ = color; }
  void set_backdrop_color(SDL_Color color) { backdrop_color_ = color; }
  void set_speed(float speed) { speed_ = speed; }
  void set_spinning_bounds(const SDL_Rect& bounds) {
    spinning_bounds_ = bounds;
  }

 protected:
  void Paint() override;

 private:
  MainWindow* main_window_ = nullptr;
  int circle_count_ = 12;
  SDL_Color color_ = {255, 255, 255, 255};
  SDL_Color backdrop_color_ = {0, 0, 0, 255};
  float speed_ = 0.005f;
  SDL_Rect spinning_bounds_ = {3, 3, 20, 20};

  Timer timer_;
  bool first_paint_ = true;
};

#endif  // UI_WIDGETS_LOADING_WIDGET_H_