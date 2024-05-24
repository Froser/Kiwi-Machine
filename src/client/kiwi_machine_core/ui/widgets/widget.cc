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

#include "ui/widgets/widget.h"

#include <imgui.h>
#include <algorithm>

#include "ui/window_base.h"

Widget::Widget(WindowBase* window) : window_(window) {}

Widget::~Widget() = default;

SDL_Rect Widget::GetLocalBounds() {
  return SDL_Rect{0, 0, bounds().w, bounds().h};
}

void Widget::SetZOrder(int zorder) {
  zorder_ = zorder;
  if (parent())
    parent()->ChildZOrderChanged(this);
}

void Widget::AddWidget(std::unique_ptr<Widget> widget) {
  widget->set_parent(this);
  widgets_.insert(std::move(widget));
}

void Widget::RemoveWidget(Widget* widget) {
  pending_remove_.push_back(widget);
}

void Widget::ChildZOrderChanged(Widget* child) {
  auto target = std::find_if(
      children().begin(), children().end(),
      [child](const std::unique_ptr<Widget>& c) { return c.get() == child; });

  Widget* isolate_child =
      const_cast<std::unique_ptr<Widget>&>(*target).release();
  children().erase(target);

  std::unique_ptr<Widget> temp(isolate_child);
  AddWidget(std::move(temp));
}

void Widget::Render() {
  if (visible()) {
    // A widget will have a window if following all conditions meet:
    // 1. It is not windowless.
    // 2. It has no parent.
    // Or:
    // Its parent is windowless, and this widget is not windowless.
    bool has_window = (!parent_ && !IsWindowless()) ||
                      (parent_ && parent_->IsWindowless() && !IsWindowless());
    bool bounds_changed = bounds_changed_;
    if (has_window) {
      // If a widget doesn't have a parent, it has to create its window.
      if (bounds_changed_) {
        // If user changed the bounds by set_bounds(), use it as the default
        // bounds when rendering.
        SDL_Rect bounds_to_parent = MapToParent(bounds_);
        ImGui::SetNextWindowPos(ImVec2(bounds_to_parent.x, bounds_to_parent.y),
                                ImGuiCond_Once);
        if (bounds_to_parent.w > 0 && bounds_to_parent.h > 0) {
          ImGui::SetNextWindowSize(
              ImVec2(bounds_to_parent.w, bounds_to_parent.h));
        }
        bounds_changed_ = false;
      }

      OnWindowPreRender();
      if (!ImGui::Begin(title().c_str(), &visible_, flags_)) {
        ImGui::End();
        OnWindowPostRender();
        return;
      }

      if (first_window_show_) {
        init_window_size_ = ImGui::GetWindowSize();
        first_window_show_ = false;
      }
    }

    Paint();

    for (const auto& widget : widgets_) {
      if (widget->visible())
        widget->Render();
    }

    PostPaint();

    RemovePendingWidgets();

    if (!bounds_changed) {
      if (flags_ & ImGuiWindowFlags_AlwaysAutoResize) {
        SDL_Rect render_rect = window()->GetClientBounds();
        ImVec2 window_size = ImGui::GetWindowSize();
        if (window_size.x != init_window_size_.x ||
            window_size.y != init_window_size_.y) {
          ImGui::SetWindowPos(ImVec2((render_rect.w - window_size.x) / 2,
                                     (render_rect.h - window_size.y) / 2),
                              ImGuiCond_Once);
        }
      }
    }

    if (has_window) {
      ImGui::End();
      OnWindowPostRender();
    }
  }
}

SDL_Rect Widget::MapToParent(const SDL_Rect& bounds) {
  SDL_Rect bounds_to_parent;

  // There's no parent, accumulates it with client bounds
  if (!parent()) {
    SDL_Rect client_bounds = window()->GetClientBounds();
    bounds_to_parent = {bounds.x + client_bounds.x, bounds.y + client_bounds.y,
                        bounds.w, bounds.h};
  } else {
    bounds_to_parent = {parent()->bounds().x + bounds.x,
                        parent()->bounds().y + bounds.y, bounds.w, bounds.h};

    bounds_to_parent = parent()->MapToParent(bounds_to_parent);
  }

  return bounds_to_parent;
}

void Widget::Paint() {}

void Widget::PostPaint() {}

bool Widget::IsWindowless() {
  return false;
}

bool Widget::HandleKeyEvents(SDL_KeyboardEvent* event) {
  bool handled = false;
  switch (event->type) {
    case SDL_KEYDOWN: {
      if (visible() && enabled()) {
        handled = OnKeyPressed(event);
        if (!handled) {
          for (auto& widget : widgets_) {
            handled = widget->HandleKeyEvents(event);
            if (handled)
              break;
          }
        }
      }
      break;
    }
    case SDL_KEYUP: {
      if (visible() && enabled()) {
        handled = OnKeyReleased(event);
        if (!handled) {
          for (auto& widget : widgets_) {
            handled = widget->HandleKeyEvents(event);
            if (handled)
              break;
          }
        }
      }
      break;
    }
    default:
      break;
  }

  return handled;
}

bool Widget::HandleJoystickAxisMotionEvents(SDL_ControllerAxisEvent* event) {
  bool handled = false;
  if (visible() && enabled()) {
    handled = OnControllerAxisMotionEvents(event);
    if (!handled) {
      for (auto& widget : widgets_) {
        handled = widget->HandleJoystickAxisMotionEvents(event);
        if (handled)
          break;
      }
    }
  }
  return handled;
}

bool Widget::HandleJoystickButtonEvents(SDL_ControllerButtonEvent* event) {
  bool handled = false;
  switch (event->type) {
    case SDL_CONTROLLERBUTTONDOWN: {
      if (visible() && enabled()) {
        handled = OnControllerButtonPressed(event);
        for (auto& widget : widgets_) {
          if (widget->HandleJoystickButtonEvents(event))
            break;
        }
      }
      break;
    }
    case SDL_CONTROLLERBUTTONUP: {
      if (visible() && enabled()) {
        handled = OnControllerButtonReleased(event);
        for (auto& widget : widgets_) {
          if (widget->HandleJoystickButtonEvents(event))
            break;
        }
      }
      break;
    }
    default:
      break;
  }

  return handled;
}

void Widget::HandleResizedEvent() {
  OnWindowResized();

  for (auto& widget : widgets_) {
    widget->HandleResizedEvent();
  }
}

void Widget::HandleDisplayEvent() {
  OnDisplayChanged();

  for (auto& widget : widgets_) {
    widget->HandleDisplayEvent();
  }
}

bool Widget::HandleTouchFingerEvent(SDL_TouchFingerEvent* event) {
  bool handled = false;
  if (enabled() && visible()) {
    if (event->type == SDL_FINGERUP)
      handled = OnTouchFingerUp(event);
    else if (event->type == SDL_FINGERDOWN)
      handled = OnTouchFingerDown(event);
    else if (event->type == SDL_FINGERMOTION)
      handled = OnTouchFingerMove(event);
    else
      SDL_assert(false);

    if (!handled) {
      for (auto& widget : widgets_) {
        handled = widget->HandleTouchFingerEvent(event);
        if (handled)
          break;
      }
    }
  }

  return handled;
}

void Widget::HandleLocaleChanged() {
  for (auto& widget : widgets_) {
    widget->HandleLocaleChanged();
  }
  OnLocaleChanged();
}

void Widget::RemovePendingWidgets() {
  bool anything_removed = false;
  for (auto iter = children().begin(); iter != children().end(); ++iter) {
    auto widget_iter =
        std::find(pending_remove_.begin(), pending_remove_.end(), iter->get());
    if (widget_iter != pending_remove_.end()) {
      iter = children().erase(iter);
      anything_removed = true;
    }

    if (iter == children().end())
      break;
  }
  pending_remove_.clear();

  if (anything_removed) {
    OnWidgetsRemoved();
  }
}

bool Widget::OnKeyPressed(SDL_KeyboardEvent* event) {
  return false;
}

bool Widget::OnKeyReleased(SDL_KeyboardEvent* event) {
  return false;
}

bool Widget::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return false;
}

bool Widget::OnControllerButtonReleased(SDL_ControllerButtonEvent* event) {
  return false;
}

bool Widget::OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) {
  return false;
}

void Widget::OnWindowResized() {}

void Widget::OnWidgetsRemoved() {}

void Widget::OnDisplayChanged() {}

void Widget::OnLocaleChanged() {}

void Widget::OnWindowPreRender() {}

void Widget::OnWindowPostRender() {}

bool Widget::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  return false;
}

bool Widget::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  return false;
}

bool Widget::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  return false;
}