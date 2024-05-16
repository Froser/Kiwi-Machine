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

#include "ui/widgets/side_menu.h"

#include "models/nes_runtime.h"
#include "ui/main_window.h"
#include "utility/audio_effects.h"
#include "utility/fonts.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"

constexpr int kItemPaddingBottom = 15;
constexpr int kItemPaddingSide = 3;

SideMenu::SideMenu(MainWindow* main_window, NESRuntimeID runtime_id)
    : Widget(main_window), main_window_(main_window) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("SideMenu");
}

SideMenu::~SideMenu() = default;

void SideMenu::Paint() {
  SDL_Rect global_bounds = MapToParent(bounds());
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(global_bounds.x, global_bounds.y),
      ImVec2(global_bounds.x + global_bounds.w,
             global_bounds.y + global_bounds.h),
      ImColor(171, 238, 80));

  const int kX = global_bounds.x + kItemPaddingSide;
  int y = 0;
  for (int i = menu_items_.size() - 1; i >= 0; --i) {
    PreferredFontSize font_size(PreferredFontSize::k1x);
    std::string menu_content = menu_items_[i].first->GetLocalizedString();
    ScopedFont font = GetPreferredFont(font_size, menu_content.c_str());
    ImVec2 text_size = ImGui::CalcTextSize(menu_content.c_str());
    const int kItemHeight = text_size.y;
    if (i == menu_items_.size() - 1) {
      y = global_bounds.h - kItemPaddingBottom - kItemHeight;
    }
    if (i == current_index_) {
      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(kX, y),
          ImVec2(kX + global_bounds.w - kItemPaddingSide, y + kItemHeight),
          ImColor(255, 255, 255));
      ImGui::GetWindowDrawList()->AddText(
          font.GetFont(), font.GetFont()->FontSize, ImVec2(kX, y),
          ImColor(171, 238, 80), menu_content.c_str());
    } else {
      ImGui::GetWindowDrawList()->AddText(
          font.GetFont(), font.GetFont()->FontSize, ImVec2(kX, y),
          ImColor(255, 255, 255), menu_content.c_str());
    }
    y -= kItemHeight;
  }
}

void SideMenu::AddMenu(std::unique_ptr<LocalizedStringUpdater> string_updater,
                       kiwi::base::RepeatingClosure callback) {
  menu_items_.emplace_back(std::move(string_updater), callback);
}

bool SideMenu::HandleInputEvents(SDL_KeyboardEvent* k,
                                 SDL_ControllerButtonEvent* c) {
  if (!activate_)
    return false;

  // if (!is_finger_down_) { TODO
  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kUp, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
    int next_index = (current_index_ <= 0 ? 0 : current_index_ - 1);
    if (next_index != current_index_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      current_index_ = next_index;
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kDown, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
    int next_index =
        (current_index_ >= menu_items_.size() - 1 ? current_index_
                                                  : current_index_ + 1);
    if (next_index != current_index_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      current_index_ = next_index;
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    if (activate_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      activate_ = false;
      main_window_->ChangeFocus(MainWindow::MainFocus::kGameItems);
    }
    return true;
  }

  return false;
}

bool SideMenu::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvents(event, nullptr);
}

bool SideMenu::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvents(nullptr, event);
}

bool SideMenu::OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) {
  return HandleInputEvents(nullptr, nullptr);
}