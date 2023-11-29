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

#include "ui/widgets/group_widget.h"

#include <imgui.h>
#include <math.h>

#include "ui/main_window.h"
#include "ui/widgets/kiwi_item_widget.h"
#include "utility/audio_effects.h"
#include "utility/key_mapping_util.h"
#include "utility/math.h"

constexpr int kMoveSpeed = 200;

GroupWidget::GroupWidget(MainWindow* main_window, NESRuntimeID runtime_id)
    : Widget(main_window), main_window_(main_window) {
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("KiwiItemsWidget");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  SDL_assert(runtime_data_);
}

GroupWidget::~GroupWidget() = default;

void GroupWidget::SetCurrent(int index) {
  current_idx_ = index;
  for (const auto& w : children()) {
    w->set_enabled(false);
  }
  children()[current_idx_]->set_enabled(true);
}

void GroupWidget::RecalculateBounds() {
  animation_lerp_ = 0.f;

  for (int i = 0; i < children().size(); ++i) {
    bounds_current_.push_back(SDL_Rect{});
  }

  CalculateItemsBounds(bounds_current_);
  bounds_next_ = bounds_current_;
  ApplyItemBounds();
  animation_counter_.Start();
}

void GroupWidget::FirstFrame() {
  SetCurrent(0);
  RecalculateBounds();
  first_paint_ = false;
}

void GroupWidget::CalculateItemsBounds(std::vector<SDL_Rect>& container) {
  int top = 0;
  SDL_Rect rect_center{0, top, bounds().w, bounds().h};
  container[current_idx_] = rect_center;

  for (int i = current_idx_ - 1; i >= 0; --i) {
    top -= rect_center.h;
    container[i] = SDL_Rect{0, top, bounds().w, bounds().h};
  }

  top = 0;
  for (size_t i = current_idx_ + 1; i < children().size(); ++i) {
    top += rect_center.h;
    container[i] = SDL_Rect{0, top, bounds().w, bounds().h};
  }
}

void GroupWidget::Layout() {
  // Calculate animation lerp factor.
  float elapsed = animation_counter_.ElapsedInMillisecondsAndReset();
  if (animation_lerp_ >= 1.f) {
    bounds_current_ = bounds_next_;
    animation_lerp_ = 1.f;
    return;
  }

  animation_lerp_ += elapsed / (kMoveSpeed / main_window_->window_scale());
  if (animation_lerp_ >= 1.f)
    animation_lerp_ = 1.f;

  ApplyItemBounds();
}

void GroupWidget::ApplyItemBounds() {
  for (size_t i = 0; i < children().size(); ++i) {
    children()[i]->set_bounds(
        Lerp(bounds_current_[i], bounds_next_[i], animation_lerp_));
  }
}

void GroupWidget::ApplyItemBoundsByFinger() {
  SDL_assert(is_finger_moving_);
  SDL_Rect rect = window()->GetClientBounds();
  int dy = (finger_y_ - finger_down_y_) * rect.h;

  for (size_t i = 0; i < children().size(); ++i) {
    children()[i]->set_bounds(
        SDL_Rect{bounds_current_[i].x, bounds_current_[i].y + dy,
                 bounds_current_[i].w, bounds_current_[i].h});
  }
}

int GroupWidget::GetNearestIndexByFinger() {
  constexpr float kScrollingThreshold = .3f;
  int dy = (finger_y_ - finger_down_y_) * window()->GetClientBounds().h;
  if (dy > bounds().h * kScrollingThreshold)
    return current_idx_ > 0 ? current_idx_ - 1 : 0;

  if (dy < -bounds().h * kScrollingThreshold)
    return current_idx_ < children().size() - 1 ? current_idx_ + 1
                                                : children().size() - 1;

  return current_idx_;
}

void GroupWidget::Paint() {
  if (children().empty())
    return;

  if (first_paint_) {
    FirstFrame();
  }

  if (is_finger_moving_ && is_moving_vertically_) {
    // Do not update animation counter (by just reset it).
    animation_counter_.ElapsedInMillisecondsAndReset();
    ApplyItemBoundsByFinger();
  } else {
    Layout();
  }
}

bool GroupWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvents(event, nullptr);
}

bool GroupWidget::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvents(nullptr, event);
}

bool GroupWidget::OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) {
  return HandleInputEvents(nullptr, nullptr);
}

bool GroupWidget::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  if (!is_finger_down_) {
    is_finger_down_ = true;
    finger_id_ = event->fingerId;
    finger_x_ = finger_down_x_ = event->x;
    finger_y_ = finger_down_y_ = event->y;
    is_moving_vertically_ = false;
  }

  return false;
}

bool GroupWidget::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  if (event->fingerId == finger_id_) {
    SetCurrent(GetNearestIndexByFinger());
    for (size_t i = 0; i < children().size(); ++i) {
      bounds_next_[i] = children()[i]->bounds();
    }
    IndexChanged();
    is_finger_down_ = false;
    is_finger_moving_ = false;
    is_moving_vertically_ = false;
  }
  return false;
}

bool GroupWidget::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  if (is_finger_down_ && event->fingerId == finger_id_) {
    finger_x_ = event->x;
    finger_y_ = event->y;

    if (!is_finger_moving_) {
      SDL_Rect rect = window()->GetClientBounds();
      is_moving_vertically_ = std::abs((finger_y_ - finger_down_y_) * rect.h) >
                              std::abs((finger_x_ - finger_down_x_) * rect.w);
      is_finger_moving_ = true;
    }

    // Propagates the event to the next widget.
    if (!is_moving_vertically_)
      return false;
  }
  return true;
}

bool GroupWidget::HandleInputEvents(SDL_KeyboardEvent* k,
                                    SDL_ControllerButtonEvent* c) {
  if (!is_finger_moving_) {
    if (IsKeyboardOrControllerAxisMotionMatch(
            runtime_data_, kiwi::nes::ControllerButton::kUp, k) ||
        c && c->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
      if (current_idx_ > 0) {
        PlayEffect(audio_resources::AudioID::kSelect);
        SetCurrent(current_idx_ - 1);
        IndexChanged();
      }
      return true;
    }

    if (IsKeyboardOrControllerAxisMotionMatch(
            runtime_data_, kiwi::nes::ControllerButton::kDown, k) ||
        c && c->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
      if (current_idx_ < children().size() - 1) {
        PlayEffect(audio_resources::AudioID::kSelect);
        SetCurrent(current_idx_ + 1);
        IndexChanged();
      }
      return true;
    }
  }

  return false;
}

void GroupWidget::IndexChanged() {
  animation_lerp_ = .0f;
  bounds_current_ = bounds_next_;
  CalculateItemsBounds(bounds_next_);
}