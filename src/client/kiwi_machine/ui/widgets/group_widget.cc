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

void GroupWidget::FirstFrame() {
  SetCurrent(0);
  animation_lerp_ = 0.f;

  for (int i = 0; i < children().size(); ++i) {
    bounds_current_.push_back(SDL_Rect{});
  }

  CalculateItemsBounds(bounds_current_);
  bounds_next_ = bounds_current_;
  ApplyItemBounds();
  animation_counter_.Start();
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

void GroupWidget::Paint() {
  if (children().empty())
    return;

  if (first_paint_) {
    FirstFrame();
  }

  Layout();
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

bool GroupWidget::HandleInputEvents(SDL_KeyboardEvent* k,
                                    SDL_ControllerButtonEvent* c) {
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

  return false;
}

void GroupWidget::IndexChanged() {
  animation_lerp_ = .0f;
  bounds_current_ = bounds_next_;
  CalculateItemsBounds(bounds_next_);
}