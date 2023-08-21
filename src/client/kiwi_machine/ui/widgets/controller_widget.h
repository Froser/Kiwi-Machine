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

#ifndef UI_WIDGETS_CONTROLLER_WIDGET_H_
#define UI_WIDGETS_CONTROLLER_WIDGET_H_

#include <string>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"

class StackWidget;
class MainWindow;
class ControllerWidget : public Widget {
 public:
  explicit ControllerWidget(MainWindow* main_window,
                            StackWidget* parent,
                            NESRuntimeID runtime_id);
  ~ControllerWidget() override;

 private:
  void Close();

 protected:
  // Widget:
  void Paint() override;
  void OnWindowResized() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) override;

 private:
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  // Switch |player|'s joystick to the next one, maybe none.
  void SwitchGameController(int player);
  void ReverseGameControllerAB(int player);

 private:
  NESRuntime::Data* runtime_data_ = nullptr;
  StackWidget* parent_ = nullptr;
  MainWindow* main_window_ = nullptr;
  int selected_player_ = 0;
};

#endif  // UI_WIDGETS_CONTROLLER_WIDGET_H_