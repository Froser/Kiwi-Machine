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

#include "ui/main_window.h"

#include <SDL.h>

#include "ui/widgets/canvas.h"
#include "ui/widgets/kiwi_items_widget.h"
#include "ui/widgets/touch_button.h"
#include "ui/widgets/virtual_joystick.h"

namespace {
constexpr int kDefaultWindowWidth = Canvas::kNESFrameDefaultWidth;
constexpr int kDefaultWindowHeight = Canvas::kNESFrameDefaultHeight;
}  // namespace

void MainWindow::CreateVirtualTouchButtons() {
  SDL_assert(main_items_widget_);
  {
    std::unique_ptr<TouchButton> vtb_start = std::make_unique<TouchButton>(
        this, image_resources::ImageID::kVtbStart);
    vtb_start_ = vtb_start.get();
    vtb_start->set_opacity(1.f);
    vtb_start->set_trigger_callback(
        kiwi::base::BindRepeating(&KiwiItemsWidget::TriggerCurrentItem,
                                  kiwi::base::Unretained(main_items_widget_)));
    AddWidget(std::move(vtb_start));
  }

  {
    std::unique_ptr<TouchButton> vtb_select = std::make_unique<TouchButton>(
        this, image_resources::ImageID::kVtbSelect);
    vtb_select_ = vtb_select.get();
    vtb_select->set_opacity(1.f);
    vtb_select->set_trigger_callback(
        kiwi::base::BindRepeating(&KiwiItemsWidget::SwapCurrentItem,
                                  kiwi::base::Unretained(main_items_widget_)));
    AddWidget(std::move(vtb_select));
  }

  {
    std::unique_ptr<VirtualJoystick> vtb_joystick =
        std::make_unique<VirtualJoystick>(this);
    vtb_joystick_ = vtb_joystick.get();
    vtb_joystick->set_visible(false);
    vtb_joystick->set_joystick_callback(kiwi::base::BindRepeating(
        &MainWindow::OnVirtualJoystickChanged, kiwi::base::Unretained(this)));
    AddWidget(std::move(vtb_joystick));
  }

  {
    std::unique_ptr<TouchButton> vtb_a =
        std::make_unique<TouchButton>(this, image_resources::ImageID::kVtbA);
    vtb_a_ = vtb_a.get();
    vtb_a->set_finger_down_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kA, true));
    vtb_a->set_trigger_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kA, false));
    vtb_a->set_visible(false);
    AddWidget(std::move(vtb_a));
  }

  {
    std::unique_ptr<TouchButton> vtb_b =
        std::make_unique<TouchButton>(this, image_resources::ImageID::kVtbB);
    vtb_b_ = vtb_b.get();
    vtb_b->set_finger_down_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kB, true));
    vtb_b->set_trigger_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kB, false));
    vtb_b->set_visible(false);
    AddWidget(std::move(vtb_b));
  }

  {
    std::unique_ptr<TouchButton> vtb_ab =
        std::make_unique<TouchButton>(this, image_resources::ImageID::kVtbAb);
    vtb_ab_ = vtb_ab.get();
    vtb_ab->set_finger_down_callback(
        kiwi::base::BindRepeating(&MainWindow::SetVirtualJoystickButton,
                                  kiwi::base::Unretained(this), 0,
                                  kiwi::nes::ControllerButton::kA, true)
            .Then(kiwi::base::BindRepeating(
                &MainWindow::SetVirtualJoystickButton,
                kiwi::base::Unretained(this), 0,
                kiwi::nes::ControllerButton::kB, true)));
    vtb_ab->set_trigger_callback(
        kiwi::base::BindRepeating(&MainWindow::SetVirtualJoystickButton,
                                  kiwi::base::Unretained(this), 0,
                                  kiwi::nes::ControllerButton::kA, false)
            .Then(kiwi::base::BindRepeating(
                &MainWindow::SetVirtualJoystickButton,
                kiwi::base::Unretained(this), 0,
                kiwi::nes::ControllerButton::kB, false)));
    vtb_ab->set_visible(false);
    AddWidget(std::move(vtb_ab));
  }

  {
    std::unique_ptr<TouchButton> vtb_b =
        std::make_unique<TouchButton>(this, image_resources::ImageID::kVtbB);
    vtb_b_ = vtb_b.get();
    vtb_b->set_finger_down_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kB, true));
    vtb_b->set_trigger_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kB, false));
    vtb_b->set_visible(false);
    AddWidget(std::move(vtb_b));
  }

  {
    constexpr float kScaling = .4f;
    {
      std::unique_ptr<TouchButton> vtb_select_bar =
          std::make_unique<TouchButton>(
              this, image_resources::ImageID::kVtbSelectBar);
      vtb_select_bar_ = vtb_select_bar.get();
      vtb_select_bar->set_finger_down_callback(kiwi::base::BindRepeating(
          &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this),
          0, kiwi::nes::ControllerButton::kSelect, true));
      vtb_select_bar->set_trigger_callback(kiwi::base::BindRepeating(
          &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this),
          0, kiwi::nes::ControllerButton::kSelect, false));
      SDL_Rect bounds = vtb_select_bar->bounds();
      bounds.w *= window_scale() * kScaling;
      bounds.h *= window_scale() * kScaling;
      vtb_select_bar->set_opacity(.3f);
      vtb_select_bar->set_bounds(bounds);
      vtb_select_bar->set_visible(false);
      AddWidget(std::move(vtb_select_bar));
    }

    {
      std::unique_ptr<TouchButton> vtb_start_bar =
          std::make_unique<TouchButton>(this,
                                        image_resources::ImageID::kVtbStartBar);
      vtb_start_bar_ = vtb_start_bar.get();
      vtb_start_bar->set_finger_down_callback(kiwi::base::BindRepeating(
          &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this),
          0, kiwi::nes::ControllerButton::kStart, true));
      vtb_start_bar->set_trigger_callback(kiwi::base::BindRepeating(
          &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this),
          0, kiwi::nes::ControllerButton::kStart, false));
      SDL_Rect bounds = vtb_start_bar->bounds();
      bounds.w *= window_scale() * kScaling;
      bounds.h *= window_scale() * kScaling;
      vtb_start_bar->set_opacity(.3f);
      vtb_start_bar->set_bounds(bounds);
      vtb_start_bar->set_visible(false);
      AddWidget(std::move(vtb_start_bar));
    }

    {
      std::unique_ptr<TouchButton> vtb_pause = std::make_unique<TouchButton>(
          this, image_resources::ImageID::kVtbPause);
      vtb_pause_ = vtb_pause.get();
      vtb_pause->set_trigger_callback(kiwi::base::BindRepeating(
          &MainWindow::OnInGameMenuTrigger, kiwi::base::Unretained(this)));
      vtb_pause->set_visible(false);
      AddWidget(std::move(vtb_pause));
    }
  }
}

void MainWindow::SetVirtualTouchButtonVisible(VirtualTouchButton button,
                                              bool visible) {
#if defined(ANDROID)
  switch (button) {
    case VirtualTouchButton::kStart:
      if (vtb_start_)
        vtb_start_->set_visible(visible);
      break;
    case VirtualTouchButton::kSelect:
      if (vtb_select_)
        vtb_select_->set_visible(visible);
      break;
    case VirtualTouchButton::kJoystick:
      if (vtb_joystick_)
        vtb_joystick_->set_visible(visible);
      break;
    case VirtualTouchButton::kA:
      if (vtb_a_)
        vtb_a_->set_visible(visible);
      break;
    case VirtualTouchButton::kB:
      if (vtb_b_)
        vtb_b_->set_visible(visible);
      break;
    case VirtualTouchButton::kAB:
      if (vtb_ab_)
        vtb_ab_->set_visible(visible);
      break;
    case VirtualTouchButton::kSelectBar:
      if (vtb_select_bar_)
        vtb_select_bar_->set_visible(visible);
      break;
    case VirtualTouchButton::kStartBar:
      if (vtb_start_bar_)
        vtb_start_bar_->set_visible(visible);
      break;
    case VirtualTouchButton::kPause:
      if (vtb_pause_)
        vtb_pause_->set_visible(visible);
      break;
    default:
      SDL_assert(false);
      break;
  }
#endif
}

void MainWindow::LayoutVirtualTouchButtons() {
  const SDL_Rect kClientBounds = GetClientBounds();
  {
    const int kSize = 33 * window_scale();
    const int kPadding = 15 * window_scale();

    if (vtb_start_) {
      SDL_Rect bounds;
      bounds.h = bounds.w = kSize;
      bounds.x = kClientBounds.w - bounds.w - kPadding;
      bounds.y = kClientBounds.h - bounds.h - kPadding;
      vtb_start_->set_bounds(bounds);
    }

    if (vtb_select_) {
      SDL_Rect bounds;
      bounds.h = bounds.w = kSize;
      bounds.x = kClientBounds.w - bounds.w * 2 - kPadding * 2;
      bounds.y = kClientBounds.h - bounds.h - kPadding;
      vtb_select_->set_bounds(bounds);
    }
  }

  {
    const int kSize = 135 * window_scale();
    const int kPadding = 18 * window_scale();

    if (vtb_joystick_) {
      SDL_Rect bounds;
      bounds.h = bounds.w = kSize;
      bounds.x = kPadding;
      bounds.y = kClientBounds.h - bounds.h - kPadding;
      vtb_joystick_->set_bounds(bounds);
    }
  }

  {
    const int kSize = 33 * window_scale();
    const int kPadding = 60 * window_scale();
    const int kSpacing = 15 * window_scale();
    if (vtb_a_) {
      SDL_Rect bounds;
      bounds.h = bounds.w = kSize;
      bounds.x = kClientBounds.w - bounds.w - kPadding;
      bounds.y = kClientBounds.h - bounds.h - kPadding;
      vtb_a_->set_bounds(bounds);
    }

    if (vtb_b_) {
      SDL_Rect bounds;
      bounds.h = bounds.w = kSize;
      bounds.x = kClientBounds.w - bounds.w * 2 - kPadding - kSpacing;
      bounds.y = kClientBounds.h - bounds.h - kPadding;
      vtb_b_->set_bounds(bounds);
    }

    if (vtb_ab_) {
      SDL_Rect bounds;
      bounds.h = bounds.w = kSize;
      bounds.x = kClientBounds.w - bounds.w - kPadding;
      bounds.y = kClientBounds.h - bounds.h * 2 - kPadding - kSpacing;
      vtb_ab_->set_bounds(bounds);
    }
  }

  {
    const int kMiddleSpacing = 4 * window_scale();
    const int kPaddingBottom = 30 * window_scale();
    if (vtb_select_bar_) {
      SDL_Rect bounds = vtb_select_bar_->bounds();
      bounds.x = kClientBounds.w / 2 - bounds.w - kMiddleSpacing;
      bounds.y = kClientBounds.h - bounds.h - kPaddingBottom;
      vtb_select_bar_->set_bounds(bounds);
    }

    if (vtb_start_bar_) {
      SDL_Rect bounds = vtb_select_bar_->bounds();
      bounds.x = kClientBounds.w / 2 + kMiddleSpacing;
      bounds.y = kClientBounds.h - bounds.h - kPaddingBottom;
      vtb_start_bar_->set_bounds(bounds);
    }
  }

  if (vtb_pause_) {
    const int kPadding = 33 * window_scale();
    const int kSize = 33 * window_scale();
    SDL_Rect bounds;
    bounds.h = bounds.w = kSize;
    bounds.x = bounds.y = kPadding;
    vtb_pause_->set_bounds(bounds);
  }
}

void MainWindow::OnVirtualJoystickChanged(int state) {
  SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kLeft, false);
  SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kRight, false);
  SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kUp, false);
  SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kDown, false);

  if (state & VirtualJoystick::kLeft)
    SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kLeft, true);
  if (state & VirtualJoystick::kRight)
    SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kRight, true);
  if (state & VirtualJoystick::kUp)
    SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kUp, true);
  if (state & VirtualJoystick::kDown)
    SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kDown, true);
}

void MainWindow::OnInGameSettingsHandleWindowSize(bool is_left) {
  if (config_->data().is_stretch_mode && !is_left)
    return;

  if (config_->data().is_stretch_mode && is_left) {
    config_->data().is_stretch_mode = false;
    config_->SaveConfig();
    OnScaleModeChanged();
  } else if (!config_->data().is_stretch_mode && !is_left) {
    config_->data().is_stretch_mode = true;
    config_->SaveConfig();
    OnScaleModeChanged();
  }
}

void MainWindow::OnScaleModeChanged() {
  if (canvas_) {
    if (config_->data().is_stretch_mode) {
      SDL_Rect rect = GetClientBounds();
      float canvas_scale = 1.f;
      if (rect.w > rect.h) {
        float scale = static_cast<float>(rect.h) / kDefaultWindowWidth;
        canvas_->set_frame_scale(scale);
      } else {
        float scale = static_cast<float>(rect.h) / kDefaultWindowWidth;
        canvas_->set_frame_scale(scale);
      }
    } else {
      canvas_->set_frame_scale(1.f);
    }
  }
}