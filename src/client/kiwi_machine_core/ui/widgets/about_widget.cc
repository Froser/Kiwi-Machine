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

#include "ui/widgets/about_widget.h"

#include <SDL_image.h>
#include <imgui.h>
#include <nes/types.h>

#include "ui/main_window.h"
#include "ui/widgets/stack_widget.h"
#include "ui/window_base.h"
#include "utility/audio_effects.h"
#include "utility/images.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"
#include "utility/text_content.h"

AboutWidget::AboutWidget(MainWindow* main_window,
                         StackWidget* parent,
                         NESRuntimeID runtime_id)
    : Widget(main_window), parent_(parent), main_window_(main_window) {
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_NoInputs;
  set_flags(window_flags);
  set_title("About");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);

  str_title_ = GetLocalizedString(string_resources::IDR_ABOUT_TITLE);
  font_title_ =
      GetPreferredFontType(PreferredFontSize::k2x, str_title_.c_str());
  str_contents_ = GetLocalizedString(string_resources::IDR_ABOUT_CONTENTS);
  font_contents_ =
      GetPreferredFontType(PreferredFontSize::k1x, str_contents_.c_str());
#if !KIWI_MOBILE
  str_go_back_ = GetLocalizedString(string_resources::IDR_ABOUT_GO_BACK);
#else
  str_go_back_ = GetLocalizedString(string_resources::IDR_ABOUT_GO_BACK_MOBILE);
#endif
  font_go_back_ =
      GetPreferredFontType(PreferredFontSize::k1x, str_go_back_.c_str());
}

AboutWidget::~AboutWidget() = default;

void AboutWidget::Close() {
  SDL_assert(parent_);
  parent_->PopWidget();
}

void AboutWidget::Paint() {
  SDL_Rect client_bounds = window()->GetClientBounds();
  set_bounds(SDL_Rect{0, 0, client_bounds.w, client_bounds.h});

  TextContent content(this);
  content.AddContent(font_title_, str_title_.c_str());
  content.AddContent(font_contents_, str_contents_.c_str());
  content.AddContent(font_go_back_, str_go_back_.c_str());
  content.DrawContents(IM_COL32_BLACK);
}

void AboutWidget::OnWindowResized() {
  set_bounds(window()->GetClientBounds());
}

bool AboutWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvents(event, nullptr);
}

bool AboutWidget::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvents(nullptr, event);
}

#if KIWI_MOBILE
bool AboutWidget::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  PlayEffect(audio_resources::AudioID::kBack);
  Close();
  return true;
}
#endif

bool AboutWidget::HandleInputEvents(SDL_KeyboardEvent* k,
                                    SDL_ControllerButtonEvent* c) {
  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kB, k) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_B)) {
    PlayEffect(audio_resources::AudioID::kBack);
    Close();
    return true;
  }

  return false;
}
