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

#include "ui/window_base.h"

#include <SDL.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>

#include "build/kiwi_defines.h"
#include "ui/application.h"

WindowBase::WindowBase(const std::string& title) {
#if !KIWI_MOBILE
  window_ =
      SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1, 1, SDL_WINDOW_ALLOW_HIGHDPI);
#elif KIWI_IOS
  window_ = SDL_CreateWindow(
      nullptr, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1, 1,
      SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN |
          SDL_WINDOW_BORDERLESS | SDL_WINDOW_MAXIMIZED);
#else
  window_ = SDL_CreateWindow(
      nullptr, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1, 1,
      SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_SHOWN);
#endif
  SDL_assert(window_);

  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
  SDL_assert(renderer_);

  Application::Get()->AddWindowToEventHandler(this);

  ImGui_ImplSDL2_InitForSDLRenderer(window_, renderer_);
  ImGui_ImplSDLRenderer2_Init(renderer_);
}

WindowBase::~WindowBase() {
  Application::Get()->RemoveWindowFromEventHandler(this);
  SDL_DestroyRenderer(renderer_);
  SDL_DestroyWindow(window_);
}

void WindowBase::SetTitle(const std::string& title) {
  title_ = title;
  SDL_SetWindowTitle(native_window(), title.c_str());
}

void WindowBase::AddWidget(std::unique_ptr<Widget> widget) {
  SDL_assert(widget);
  // Adding widget during rendering is not allowed. It may cause widgets
  // reallocation and crash.
  SDL_assert(!is_rendering_);
  widgets_.push_back(std::move(widget));
}

void WindowBase::RemoveWidgetLater(Widget* widget) {
  // Move the target |widget| to the removing pending list. It will be removed
  // after paint.
  for (auto iter = widgets_.begin(); iter != widgets_.end(); ++iter) {
    if (iter->get() == widget) {
      widgets_to_be_removed_.insert(widget);
      return;
    }
  }
}

void WindowBase::RenderWidgets() {
  SDL_SetRenderDrawColor(renderer_, 0x00, 0x00, 0x00, 0x00);

  if (!widgets_.empty()) {
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    for (const auto& widget : widgets_) {
      widget->Render();
    }

    ImGui::Render();
    ImGuiIO& io = ImGui::GetIO();
    SDL_RenderSetScale(renderer_, io.DisplayFramebufferScale.x,
                       io.DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData());
    SDL_RenderPresent(renderer_);
  }
}

void WindowBase::MoveToCenter() {
  int current_width, current_height;
  SDL_GetWindowSize(window_, &current_width, &current_height);
  SDL_Rect display_bounds;
  SDL_GetDisplayBounds(0, &display_bounds);
  SDL_SetWindowPosition(
      window_, display_bounds.x + (display_bounds.w - current_width) / 2,
      display_bounds.y + (display_bounds.h - current_height) / 2);
}

void WindowBase::Hide() {
  SDL_HideWindow(window_);
}

void WindowBase::Show() {
  SDL_ShowWindow(window_);
}

uint32_t WindowBase::GetWindowID() {
  SDL_assert(window_);
  return SDL_GetWindowID(window_);
}

void WindowBase::Resize(int width, int height) {
  SDL_assert(window_);
  int current_width, current_height;
  SDL_GetWindowSize(window_, &current_width, &current_height);
  if (current_width != width && current_height != height) {
    SDL_SetWindowSize(window_, width, height);
  }
}

void WindowBase::HandleKeyEvent(SDL_KeyboardEvent* event) {
  switch (event->type) {
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      for (auto& widget : widgets_) {
        if (widget->visible() && widget->enabled() &&
            widget->HandleKeyEvent(event)) {
          break;
        }
      }
      break;
    }
    default:
      break;
  }
}

void WindowBase::HandleJoystickButtonEvent(SDL_ControllerButtonEvent* event) {
  switch (event->type) {
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
      for (auto& widget : widgets_) {
        if (widget->visible() && widget->enabled() &&
            widget->HandleJoystickButtonEvent(event)) {
          break;
        }
      }
    } break;
    default:
      break;
  }
}

void WindowBase::HandleJoystickAxisMotionEvent(SDL_ControllerAxisEvent* event) {
  for (auto& widget : widgets_) {
    if (widget->visible() && widget->enabled() &&
        widget->HandleJoystickAxisMotionEvent(event)) {
      break;
    }
  }
}

void WindowBase::HandleMouseMoveEvent(SDL_MouseMotionEvent* event) {
  for (auto& widget : widgets_) {
    if (widget->HandleMouseMoveEvent(event))
      break;
  }
}

void WindowBase::HandleMouseWheelEvent(SDL_MouseWheelEvent* event) {
  for (auto& widget : widgets_) {
    if (widget->HandleMouseWheelEvent(event))
      break;
  }
}

void WindowBase::HandleJoystickDeviceEvent(SDL_ControllerDeviceEvent* event) {
  switch (event->type) {
    case SDL_CONTROLLERDEVICEADDED:
      OnControllerDeviceAdded(event);
      break;
    case SDL_CONTROLLERDEVICEREMOVED:
      OnControllerDeviceRemoved(event);
    default:
      break;
  }
}

void WindowBase::HandleResizedEvent() {
  for (auto& widget : widgets_) {
    widget->HandleResizedEvent();
  }
}

void WindowBase::HandleDisplayEvent(SDL_DisplayEvent* event) {
  for (auto& widget : widgets_) {
    widget->HandleDisplayEvent();
  }
}

SDL_Rect WindowBase::GetClientBounds() {
  SDL_Rect rect = {0};
  SDL_GetWindowSize(window_, &rect.w, &rect.h);
  return rect;
}

void WindowBase::OnControllerDeviceAdded(SDL_ControllerDeviceEvent* event) {}

void WindowBase::OnControllerDeviceRemoved(SDL_ControllerDeviceEvent* event) {}

void WindowBase::Render() {
  is_rendering_ = true;
  SDL_RenderClear(renderer_);
  RenderWidgets();
  is_rendering_ = false;

  RemovePendingWidgets();
}

void WindowBase::HandlePostEvent() {}

void WindowBase::HandleTouchFingerEvent(SDL_TouchFingerEvent* event) {
  for (auto& widget : widgets_) {
    if (widget->visible() && widget->enabled() &&
        widget->HandleTouchFingerEvent(event)) {
      break;
    }
  }
}

void WindowBase::HandleLocaleChanged() {
  for (auto& widget : widgets_) {
    widget->HandleLocaleChanged();
  }
}

void WindowBase::RemovePendingWidgets() {
  for (Widget* widget : widgets_to_be_removed_) {
    for (auto iter = widgets_.begin(); iter != widgets_.end(); ++iter) {
      if (iter->get() == widget)
        iter = widgets_.erase(iter);

      if (iter == widgets_.end())
        break;
    }
  }
  widgets_to_be_removed_.clear();
}

#if !KIWI_IOS
SDL_Rect WindowBase::GetSafeAreaInsets() {
  return SDL_Rect{0};
}
#endif

SDL_Rect WindowBase::GetSafeAreaClientBounds() {
  SDL_Rect safe_area_insets = GetSafeAreaInsets();
  SDL_Rect client_bounds = GetClientBounds();
  return SDL_Rect{client_bounds.x + safe_area_insets.x,
                  client_bounds.y + safe_area_insets.y,
                  client_bounds.w - safe_area_insets.x - safe_area_insets.w,
                  client_bounds.h - safe_area_insets.y - safe_area_insets.h};
}