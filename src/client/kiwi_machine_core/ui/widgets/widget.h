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
#include <set>
#include <string>
#include <vector>

class WindowBase;
class Widget {
 private:
  struct ZOrderComparer {
    bool operator()(const std::unique_ptr<Widget>& lhs,
                    const std::unique_ptr<Widget>& rhs) const {
      return lhs->zorder() < rhs->zorder();
    }
  };

 public:
  using Widgets = std::multiset<std::unique_ptr<Widget>, ZOrderComparer>;
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

  void SetZOrder(int zorder);
  int zorder() { return zorder_; }

  const std::string& title() { return title_; }

 public:
  void AddWidget(std::unique_ptr<Widget> widget);

  // RemoveWidget() moves |widget| to the pending list. It won't be removed
  // immediately because this widget is rendering, so it won't be removed until
  // widget rendering finishing.
  // When all pending widgets are really removed, OnWidgetsRemoved() will be
  // called.
  void RemoveWidget(Widget* widget);
  void ChildZOrderChanged(Widget* child);
  void Render();
  bool HandleKeyEvent(SDL_KeyboardEvent* event);
  bool HandleJoystickButtonEvent(SDL_ControllerButtonEvent* event);
  bool HandleJoystickAxisMotionEvent(SDL_ControllerAxisEvent* event);
  bool HandleMouseMoveEvent(SDL_MouseMotionEvent* event);
  bool HandleMouseWheelEvent(SDL_MouseWheelEvent* event);
  bool HandleMousePressedEvent(SDL_MouseButtonEvent* event);
  bool HandleMouseReleasedEvent(SDL_MouseButtonEvent* event);
  bool HandleTextEditingEvent(SDL_TextEditingEvent* event);
  bool HandleTextInputEvent(SDL_TextInputEvent* event);
  void HandleResizedEvent();
  void HandleDisplayEvent();
  bool HandleTouchFingerEvent(SDL_TouchFingerEvent* event);
  void HandleLocaleChanged();

 protected:
  WindowBase* window() { return window_; }
  Widget* parent() { return parent_; }
  SDL_Rect MapToWindow(const SDL_Rect& bounds);
  Widgets& children() { return widgets_; }
  void RemovePendingWidgets();

 private:
  void set_parent(Widget* parent) { parent_ = parent; }
  Widget* HitTest(int x_in_window, int y_in_window);

 protected:
  virtual void Paint();
  virtual void PostPaint();
  virtual bool IsWindowless();
  // Returns the combination of HitTestPolicy.
  enum HitTestPolicy : int {
    kNoHitTest = 0,
    kAcceptHitTest = 1,
    kAlwaysHitTest = 2,
    kChildrenAcceptHitTest = 4,
  };
  virtual int GetHitTestPolicy();
  virtual bool OnKeyPressed(SDL_KeyboardEvent* event);
  virtual bool OnKeyReleased(SDL_KeyboardEvent* event);
  virtual bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event);
  virtual bool OnControllerButtonReleased(SDL_ControllerButtonEvent* event);
  virtual bool OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event);
  virtual bool OnMouseMove(SDL_MouseMotionEvent* event);
  virtual bool OnMouseWheel(SDL_MouseWheelEvent* event);
  virtual bool OnMousePressed(SDL_MouseButtonEvent* event);
  virtual bool OnMouseReleased(SDL_MouseButtonEvent* event);
  virtual bool OnTextEditing(SDL_TextEditingEvent* event);
  virtual bool OnTextInput(SDL_TextInputEvent* event);
  virtual void OnWindowResized();
  virtual void OnWidgetsRemoved();
  virtual void OnDisplayChanged();
  virtual void OnLocaleChanged();
  virtual void OnWindowPreRender();
  virtual void OnWindowPostRender();

  // Finger events are global events. No matter the finger is on the widget or
  // not, it will be triggered.
  virtual bool OnTouchFingerDown(SDL_TouchFingerEvent* event);
  virtual bool OnTouchFingerUp(SDL_TouchFingerEvent* event);
  virtual bool OnTouchFingerMove(SDL_TouchFingerEvent* event);

 private:
  WindowBase* window_ = nullptr;
  int flags_ = ImGuiWindowFlags_None;
  bool enabled_ = true;
  bool visible_ = true;
  SDL_Rect bounds_ = {0, 0, 0, 0};
  std::string title_;
  Widgets widgets_;
  std::vector<Widget*> pending_remove_;
  Widget* parent_ = nullptr;
  int zorder_ = 0;

  // For internal layout use:
  bool bounds_changed_ = false;
  bool first_window_show_ = true;
  ImVec2 init_window_size_;
};

#endif  // UI_WIDGETS_WIDGET_H_