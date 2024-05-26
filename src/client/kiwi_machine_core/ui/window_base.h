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

#ifndef UI_WINDOW_BASE_H_
#define UI_WINDOW_BASE_H_

#include <SDL.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "build/kiwi_defines.h"
#include "ui/widgets/widget.h"

class WindowBase {
 public:
  explicit WindowBase(const std::string& title);
  virtual ~WindowBase();

 public:
  void SetTitle(const std::string& title);
  void AddWidget(std::unique_ptr<Widget> widget);
  void RemoveWidgetLater(Widget* widget);
  void RenderWidgets();
  void MoveToCenter();
  void Hide();
  void Show();

  uint32_t GetWindowID();
  void Resize(int width, int height);
  SDL_Renderer* renderer() { return renderer_; }
  SDL_Window* native_window() { return window_; }

  // Get the insets that you use to determine the safe area for this view.
  // SDL_Rect's x, y, width and height represents left, top, right, and bottom
  // respectively.
  SDL_Rect GetSafeAreaInsets();
  SDL_Rect GetSafeAreaClientBounds();

  // Events pipeline
  virtual void HandleKeyEvent(SDL_KeyboardEvent* event);
  virtual void HandleJoystickButtonEvent(SDL_ControllerButtonEvent* event);
  virtual void HandleJoystickDeviceEvent(SDL_ControllerDeviceEvent* event);
  virtual void HandleJoystickAxisMotionEvent(SDL_ControllerAxisEvent* event);
  virtual void HandleMouseMoveEvent(SDL_MouseMotionEvent* event);
  virtual void HandleResizedEvent();
  virtual void HandleDisplayEvent(SDL_DisplayEvent* event);
  virtual void HandlePostEvent();
  virtual void HandleTouchFingerEvent(SDL_TouchFingerEvent* event);
  virtual void HandleLocaleChanged();
  virtual SDL_Rect GetClientBounds();
  virtual void Render();

 protected:
  virtual void OnControllerDeviceAdded(SDL_ControllerDeviceEvent* event);
  virtual void OnControllerDeviceRemoved(SDL_ControllerDeviceEvent* event);

 private:
  void RemovePendingWidgets();

 private:
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  bool is_rendering_ = false;
  std::vector<std::unique_ptr<Widget>> widgets_;
  std::set<Widget*> widgets_to_be_removed_;
  std::string title_;
};

#endif  // UI_WINDOW_BASE_H_