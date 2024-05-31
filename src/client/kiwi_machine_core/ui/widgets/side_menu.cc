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
#include "utility/math.h"

constexpr int kItemMarginBottom = 15;
constexpr int kItemPadding = 3;
constexpr int kItemAnimationMs = 50;
constexpr ImColor kBackgroundColor = ImColor(171, 238, 80);

SideMenu::SideMenu(MainWindow* main_window, NESRuntimeID runtime_id)
    : Widget(main_window), main_window_(main_window) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("SideMenu");
}

SideMenu::~SideMenu() = default;

void SideMenu::Paint() {
  Layout();

  SDL_Rect kGlobalBounds = MapToGlobal(bounds());
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(kGlobalBounds.x, kGlobalBounds.y),
      ImVec2(kGlobalBounds.x + kGlobalBounds.w,
             kGlobalBounds.y + kGlobalBounds.h),
      kBackgroundColor);

  for (int i = menu_items_.size() - 1; i >= 0; --i) {
    PreferredFontSize font_size(PreferredFontSize::k1x);
    std::string menu_content = menu_items_[i].first->GetLocalizedString();
    ScopedFont font = GetPreferredFont(font_size, menu_content.c_str());
    if (i == current_index_) {
      // Animation
      if (SDL_RectEmpty(&selection_current_rect_in_global_)) {
        selection_current_rect_in_global_ = items_bounds_map_[i];
      }
      selection_target_rect_in_global_ = items_bounds_map_[i];
      float percentage =
          timer_.ElapsedInMilliseconds() / static_cast<float>(kItemAnimationMs);
      if (percentage >= 1.f) {
        percentage = 1.f;
        selection_current_rect_in_global_ = selection_target_rect_in_global_;
      }

      SDL_Rect selection_rect =
          Lerp(selection_current_rect_in_global_,
               selection_target_rect_in_global_, percentage);

      ImGui::GetWindowDrawList()->AddRectFilled(
          ImVec2(selection_rect.x, selection_rect.y),
          ImVec2(selection_rect.x + selection_rect.w,
                 selection_rect.y + selection_rect.h),
          ImColor(255, 255, 255));

      ImGui::GetWindowDrawList()->AddText(
          font.GetFont(), font.GetFont()->FontSize,
          ImVec2(selection_target_rect_in_global_.x + kItemPadding,
                 selection_target_rect_in_global_.y + kItemPadding),
          kBackgroundColor, menu_content.c_str());
    } else {
      ImGui::GetWindowDrawList()->AddText(
          font.GetFont(), font.GetFont()->FontSize,
          ImVec2(items_bounds_map_[i].x + kItemPadding,
                 items_bounds_map_[i].y + kItemPadding),
          ImColor(255, 255, 255), menu_content.c_str());
    }
  }
}

void SideMenu::AddMenu(std::unique_ptr<LocalizedStringUpdater> string_updater,
                       MenuCallbacks callbacks) {
  menu_items_.emplace_back(std::move(string_updater), callbacks);
  invalidate();

  // When the first widget is added, trigger its selected callback, because it
  // is selected by default.
  if (menu_items_.size() == 1)
    menu_items_[0].second.selected_callback.Run();
}

bool SideMenu::HandleInputEvent(SDL_KeyboardEvent* k,
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
      timer_.Reset();
      menu_items_[current_index_].second.selected_callback.Run();
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
      timer_.Reset();
      menu_items_[current_index_].second.selected_callback.Run();
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    if (activate_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      menu_items_[current_index_].second.enter_callback.Run();
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kA, k) ||
      IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kStart, k)) {
    if (activate_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      menu_items_[current_index_].second.trigger_callback.Run();
    }
    return true;
  }

  return false;
}

void SideMenu::Layout() {
  if (!bounds_valid_) {
    items_bounds_map_.resize(menu_items_.size());
    SDL_Rect kGlobalBounds = MapToGlobal(bounds());
    const int kX = kGlobalBounds.x + kItemPadding;
    int y = 0;

    for (int i = menu_items_.size() - 1; i >= 0; --i) {
      PreferredFontSize font_size(PreferredFontSize::k1x);
      std::string menu_content = menu_items_[i].first->GetLocalizedString();
      ScopedFont font = GetPreferredFont(font_size, menu_content.c_str());
      ImVec2 text_size = ImGui::CalcTextSize(menu_content.c_str());
      const int kItemHeight = text_size.y;
      if (i == menu_items_.size() - 1)
        y = kGlobalBounds.h - kItemMarginBottom - kItemHeight -
            kItemPadding * 2;

      items_bounds_map_[i] = SDL_Rect{kX, y, kGlobalBounds.w - kItemPadding,
                                      kItemHeight + kItemPadding * 2};
      y -= kItemHeight + kItemPadding * 2;
    }

    bounds_valid_ = true;
  }
}

bool SideMenu::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvent(event, nullptr);
}

bool SideMenu::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvent(nullptr, event);
}

bool SideMenu::OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) {
  return HandleInputEvent(nullptr, nullptr);
}

void SideMenu::OnWindowPreRender() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
}

void SideMenu::OnWindowPostRender() {
  ImGui::PopStyleVar(2);
}