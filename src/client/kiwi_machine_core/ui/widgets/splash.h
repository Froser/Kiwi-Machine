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
#include "utility/fonts.h"
#include "utility/timer.h"

class MainWindow;
class StackWidget;
class Splash : public Widget {
 public:
  explicit Splash(MainWindow* main_window,
                  StackWidget* stack_widget,
                  NESRuntimeID runtime_id);
  ~Splash() override;

 public:
  void Play();

 protected:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;

 private:
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);

 private:
  MainWindow* main_window_ = nullptr;
  StackWidget* stack_widget_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  Timer fade_timer_;
  Timer splash_timer_;

  enum class SplashState {
    kLogo,
    kHowToPlay,
    kClosingHowToPlay,
    kIntroduction,
    kClosing,
  };
  SplashState state_ = SplashState::kLogo;
};

#endif  // UI_WIDGETS_SPLASH_H_