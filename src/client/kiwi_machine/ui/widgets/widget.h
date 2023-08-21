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

#ifndef UI_WIDGETS_WIDGET_H_
#define UI_WIDGETS_WIDGET_H_

#include <SDL.h>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

class WindowBase;
class Widget {
 public:
  explicit Widget(WindowBase* window_base);
  virtual ~Widget();

 public:
  // set Imgui widget flags:
  void set_flags(int flags) { flags_ = flags; }
  void set_title(const std::string& title) { title_ = title; }
  void set_bounds(const SDL_Rect& bounds) {
    if (!SDL_RectEquals(&bounds_, &bounds)) {
      bounds_changed_ = true;
      bounds_ = bounds;
    }
  }

  SDL_Rect bounds() { return bounds_; }
  SDL_Rect GetLocalBounds();

  void set_visible(bool visible) { visible_ = visible; }
  bool visible() { return visible_; }

  void set_enabled(bool enabled) { enabled_ = enabled; }
  bool enabled() { return enabled_; }

  const std::string& title() { return title_; }

 public:
  void AddWidget(std::unique_ptr<Widget> widget);

  // RemoveWidget() moves |widget| to the pending list. It won't be removed
  // immediately because this widget is rendering, so it won't be removed until
  // widget rendering finishing.
  // When all pending widgets are really removed, OnWidgetsRemoved() will be
  // called.
  void RemoveWidget(Widget* widget);
  void Render();
  bool HandleKeyEvents(SDL_KeyboardEvent* event);
  bool HandleJoystickButtonEvents(SDL_ControllerButtonEvent* event);
  bool HandleJoystickAxisMotionEvents(SDL_ControllerAxisEvent* event);
  void HandleResizedEvent();

 protected:
  WindowBase* window() { return window_; }
  Widget* parent() { return parent_; }
  SDL_Rect MapToParent(const SDL_Rect& bounds);
  std::vector<std::unique_ptr<Widget>>& children() { return widgets_; }
  void RemovePendingWidgets();

 private:
  void set_parent(Widget* parent) { parent_ = parent; }

 protected:
  virtual void Paint();
  virtual bool IsWindowless();
  virtual bool OnKeyPressed(SDL_KeyboardEvent* event);
  virtual bool OnKeyReleased(SDL_KeyboardEvent* event);
  virtual bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event);
  virtual bool OnControllerButtonReleased(SDL_ControllerButtonEvent* event);
  virtual bool OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event);
  virtual void OnWindowResized();
  virtual void OnWidgetsRemoved();

 private:
  WindowBase* window_ = nullptr;
  int flags_ = ImGuiWindowFlags_None;
  bool enabled_ = true;
  bool visible_ = true;
  SDL_Rect bounds_ = {0, 0, 0, 0};
  std::string title_;
  std::vector<std::unique_ptr<Widget>> widgets_;
  std::vector<Widget*> pending_remove_;
  Widget* parent_ = nullptr;

  // For internal layout use:
  bool bounds_changed_ = false;
  bool first_window_show_ = true;
  ImVec2 init_window_size_;
};

#endif  // UI_WIDGETS_WIDGET_H_