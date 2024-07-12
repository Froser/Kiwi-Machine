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

#ifndef UI_WIDGETS_ABOUT_WIDGET_H_
#define UI_WIDGETS_ABOUT_WIDGET_H_

#include <string>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/fonts.h"

class StackWidget;
class MainWindow;
class AboutWidget : public Widget {
 public:
  explicit AboutWidget(MainWindow* main_window,
                       StackWidget* parent,
                       NESRuntimeID runtime_id);
  ~AboutWidget() override;

 private:
  void Close();

 protected:
  // Widget:
  void Paint() override;
  void OnWindowResized() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  void OnWindowPreRender() override;
  void OnWindowPostRender() override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;
#if KIWI_MOBILE
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
#endif


 private:
  bool HandleInputEvent(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  void ResetCursorX();
  void Separator();
  void DrawBackground();
  void DrawTitle();
  void DrawController();
  void DrawGameSelection();
  void DrawAbout();

 private:
  NESRuntime::Data* runtime_data_ = nullptr;
  StackWidget* parent_ = nullptr;
  MainWindow* main_window_ = nullptr;
};

#endif  // UI_WIDGETS_ABOUT_WIDGET_H_