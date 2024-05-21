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

#include "ui/widgets/splash.h"

#include <cctype>
#include <tuple>
#include <vector>

#include "build/kiwi_defines.h"
#include "ui/main_window.h"
#include "ui/widgets/stack_widget.h"
#include "utility/audio_effects.h"
#include "utility/images.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"
#include "utility/math.h"

constexpr int kSplashDurationMs = 2500;
constexpr float kFadeDurationMs = 1000.f;

Splash::Splash(MainWindow* main_window,
               StackWidget* stack_widget,
               NESRuntimeID runtime_id)
    : Widget(main_window),
      main_window_(main_window),
      stack_widget_(stack_widget) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
  set_flags(window_flags);
  set_title("Splash");
}

Splash::~Splash() {
  if (closed_callback_)
    std::move(closed_callback_).Run();
}

void Splash::Play() {
  splash_timer_.Start();
  fade_timer_.Start();
  PlayEffect(audio_resources::AudioID::kStartup);
  state_ = SplashState::kLogo;
}

void Splash::SetClosedCallback(kiwi::base::RepeatingClosure callback) {
  closed_callback_ = std::move(callback);
}

void Splash::Paint() {
  constexpr float kLogoScaling = .2f;
  const ImVec2 kSplashSize(bounds().w, bounds().h);
  if (state_ == SplashState::kLogo) {
    SDL_Texture* logo =
        GetImage(window()->renderer(), image_resources::ImageID::kKiwiMachine);
    int w, h;
    SDL_QueryTexture(logo, nullptr, nullptr, &w, &h);

    w *= main_window_->window_scale() * kLogoScaling;
    h *= main_window_->window_scale() * kLogoScaling;

    ImVec2 logo_size(w, h);
    ImVec2 logo_pos((kSplashSize.x - logo_size.x) / 2,
                    (kSplashSize.y - logo_size.y) / 2);

    ImGui::SetCursorPos(logo_pos);
    ImGui::Image(logo, logo_size);

    int current_color =
        Lerp(0, 255, fade_timer_.ElapsedInMilliseconds() / kFadeDurationMs);
    ImColor bg_color(current_color, current_color, current_color);
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), kSplashSize,
                                                  bg_color);

    if (splash_timer_.ElapsedInMilliseconds() > kSplashDurationMs) {
      fade_timer_.Start();
      state_ = SplashState::kClosing;
    }
  } else {
    if (state_ == SplashState::kClosing)
      stack_widget_->PopWidget();
  }
}

bool Splash::HandleInputEvents(SDL_KeyboardEvent* k,
                               SDL_ControllerButtonEvent* c) {
  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kA, k) ||
      IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kStart, k) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_A) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_START)) {
    if (state_ == SplashState::kLogo) {
      state_ = SplashState::kClosing;
      fade_timer_.Start();
      PlayEffect(audio_resources::AudioID::kStart);
      return true;
    }
  }
  return false;
}

bool Splash::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvents(event, nullptr);
}

bool Splash::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvents(nullptr, event);
}

bool Splash::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  SDL_ControllerButtonEvent c;
  c.button = SDL_CONTROLLER_BUTTON_A;
  return HandleInputEvents(nullptr, &c);
}
