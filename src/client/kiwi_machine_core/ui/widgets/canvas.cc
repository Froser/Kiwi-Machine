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

#include "ui/widgets/canvas.h"
#include "ui/widgets/canvas_observer.h"
#include "ui/window_base.h"

#include <imgui.h>

namespace {
constexpr int kBrightThreshold = 220;
bool IsColorBrightEnough(int r, int g, int b) {
  float luminance = 0.299 * r + 0.587 * g + 0.114 * b;
  return luminance > kBrightThreshold;
}

}  // namespace

Canvas::Canvas(WindowBase* window_base, NESRuntimeID runtime_id)
    : Widget(window_base) {
  frame_ = kiwi::base::MakeRefCounted<NESFrame>(window_base, runtime_id);
  frame_->AddObserver(this);
  set_bounds(SDL_Rect{0, 0, kNESFrameDefaultWidth, kNESFrameDefaultHeight});
}

Canvas::~Canvas() = default;

void Canvas::Clear() {
  SDL_RenderClear(window()->renderer());
}

int Canvas::GetZapperState() {
  using State = kiwi::nes::IODevices::InputDevice::ZapperState;
  int state = State::kNone;
  if (mouse_or_finger_down_) {
    state |= State::kTriggered;
  }

  if (ZapperTest(Input::kMouse) || ZapperTest(Input::kFinger))
    state |= State::kLightSensed;

  return state;
}

void Canvas::AddObserver(CanvasObserver* observer) {
  observers_.insert(observer);
}

void Canvas::RemoveObserver(CanvasObserver* observer) {
  observers_.erase(observer);
}

void Canvas::Paint() {
  // Notify all observers
  for (CanvasObserver* observer : observers_) {
    observer->OnAboutToRenderFrame(this, frame_.get());
  }

  SDL_Rect src_rect = {0, 0, frame_->width(), frame_->height()};
  SDL_Rect dest_rect = bounds();
  SDL_RenderCopy(window()->renderer(), frame_->texture(), &src_rect,
                 &dest_rect);
}

bool Canvas::IsWindowless() {
  return true;
}

bool Canvas::OnKeyPressed(SDL_KeyboardEvent* event) {
  if (event->keysym.sym == SDLK_ESCAPE) {
    InvokeInGameMenu();
    return true;
  }
  return false;
}

bool Canvas::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  SDL_GameController* controller =
      SDL_GameControllerFromInstanceID(event->which);
  if (SDL_GameControllerGetButton(controller,
                                  SDL_CONTROLLER_BUTTON_LEFTSHOULDER) &&
      SDL_GameControllerGetButton(controller,
                                  SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
    InvokeInGameMenu();
    return true;
  }
  return false;
}

bool Canvas::OnMousePressed(SDL_MouseButtonEvent* event) {
  mouse_or_finger_down_ = true;
  return Widget::OnMousePressed(event);
}

bool Canvas::OnMouseReleased(SDL_MouseButtonEvent* event) {
  mouse_or_finger_down_ = false;
  return Widget::OnMouseReleased(event);
}

bool Canvas::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  mouse_or_finger_down_ = true;

  SDL_Rect bounds = window()->GetClientBounds();
  touch_point_ = std::make_pair(event->x * bounds.w, event->y * bounds.h);
  return Widget::OnTouchFingerDown(event);
}

bool Canvas::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  mouse_or_finger_down_ = false;
  touch_point_ = std::nullopt;
  return Widget::OnTouchFingerUp(event);
}

void Canvas::OnShouldRender(int since_last_frame_ms) {}

void Canvas::InvokeInGameMenu() {
  if (on_menu_trigger_)
    on_menu_trigger_.Run();
}

Canvas::ZapperDetails Canvas::CreateZapperDetailsByMouseOrFingerPosition(
    int x,
    int y) {
  // bounds here is relative to client rect, which will contain menu bar's
  // height when debug is enabled.
  int non_client_height = window()->GetClientBounds().y;
  SDL_Rect adjusted_bounds = bounds();
  adjusted_bounds.y -= non_client_height;

  SDL_Rect bounds_to_window = MapToWindow(adjusted_bounds);
  int relative_x = x - bounds_to_window.x;
  int relative_y = y - bounds_to_window.y;

  return ZapperDetails{
      relative_x * kNESFrameDefaultWidth / bounds_to_window.w,
      relative_y * kNESFrameDefaultHeight / bounds_to_window.h};
}

bool Canvas::ZapperTest(Input input) {
  ZapperDetails details;
  if (input == Input::kMouse) {
    int x, y;
    SDL_GetMouseState(&x, &y);
    details = CreateZapperDetailsByMouseOrFingerPosition(x, y);
  } else {
    // By gesture
    if (touch_point_) {
      details = CreateZapperDetailsByMouseOrFingerPosition(
          std::get<0>(*touch_point_), std::get<1>(*touch_point_));
    } else {
      return false;
    }
  }

  if (details.original_x < 0 || details.original_x >= frame_->width() ||
      details.original_y < 0 || details.original_y >= frame_->height())
    return false;

  size_t data_index = frame_->width() * details.original_y + details.original_x;
  kiwi::nes::Color color = frame_->GetCurrentFrame()[data_index];
  return IsColorBrightEnough(color & 0xff, (color >> 8) & 0xff,
                             (color >> 16) & 0xff);
}