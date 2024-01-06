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
  void InitializeStrings();
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);

 private:
  MainWindow* main_window_ = nullptr;
  StackWidget* stack_widget_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  Timer fade_timer_;
  Timer splash_timer_;

  enum class SplashState {
    kLogo,
#if !KIWI_MOBILE
    kHowToPlayKeyboard,
    kClosingHowToPlayKeyboard,
    kHowToPlayJoystick,
    kClosingHowToPlayJoystick,
#endif
    kIntroduction,
    kClosing,
  };
  SplashState state_ = SplashState::kLogo;

  // String lists
  std::string str_how_to_play_;
  FontType font_how_to_play_;
#if !KIWI_MOBILE
  std::string str_controller_instructions_;
  FontType font_controller_instructions_;
  std::string str_controller_instructions_contents_;
  FontType font_controller_instructions_contents_;
  std::string str_menu_instructions_contents_;
  FontType font_menu_instructions_contents_;
#endif
  std::string str_continue_;
  FontType font_continue_;
  std::string str_introductions_;
  FontType font_introductions_;
  std::string str_retro_collections_;
  FontType font_retro_collections_;
  std::string str_retro_collections_contents_;
  FontType font_retro_collections_contents_;
  std::string str_special_collections_;
  FontType font_special_collections_;
  std::string str_special_collections_contents_;
  FontType font_special_collections_contents_;
};

#endif  // UI_WIDGETS_SPLASH_H_