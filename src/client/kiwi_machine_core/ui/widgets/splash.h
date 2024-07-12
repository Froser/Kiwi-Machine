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

#ifndef UI_WIDGETS_SPLASH_H_
#define UI_WIDGETS_SPLASH_H_

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

class MainWindow;
class StackWidget;
class Splash : public Widget {
 public:
  explicit Splash(MainWindow* main_window);
  ~Splash() override;

 public:
  int GetElapsedMs();

 protected:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnKeyReleased(SDL_KeyboardEvent* event) override;
  void OnWindowPreRender() override;
  void OnWindowPostRender() override;

 private:
  MainWindow* main_window_ = nullptr;
  bool first_paint_ = true;
  Timer timer_;
};

#endif  // UI_WIDGETS_SPLASH_H_