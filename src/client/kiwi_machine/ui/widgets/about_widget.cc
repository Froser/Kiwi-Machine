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
  content.AddContent(FontType::kDefault2x, "About Kiwi Machine");
  content.AddContent(FontType::kDefault, R"(
Kiwi machine is an open sources NES
emulator with lots of preset games.
Github: https://github.com/Froser/Kiwi-Machine/

Core Version: Kiwi 1.0.0
UI Version: 1.0.0
Programmed by Yu Yisi

)");
  content.AddContent(FontType::kDefault,
                     "Press joystick button 'B' to go back.");

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

bool AboutWidget::HandleInputEvents(SDL_KeyboardEvent* k,
                                    SDL_ControllerButtonEvent* c) {
  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kB, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_B) {
    PlayEffect(audio_resources::AudioID::kBack);
    Close();
    return true;
  }

  return false;
}
