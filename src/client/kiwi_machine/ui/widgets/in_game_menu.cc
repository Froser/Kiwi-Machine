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

#include "ui/widgets/in_game_menu.h"

#include <climits>
#include <imgui.h>

#include "ui/main_window.h"
#include "ui/widgets/canvas.h"
#include "utility/audio_effects.h"
#include "utility/fonts.h"
#include "utility/key_mapping_util.h"

constexpr int kMoveSpeed = 200;

InGameMenu::InGameMenu(MainWindow* main_window,
                       NESRuntimeID runtime_id,
                       MenuItemCallback menu_callback,
                       SettingsItemCallback settings_callback)
    : Widget(main_window),
      main_window_(main_window),
      menu_callback_(menu_callback),
      settings_callback_(settings_callback) {
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("InGameMenu");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  SDL_assert(runtime_data_);
}

InGameMenu::~InGameMenu() {
  if (snapshot_)
    SDL_DestroyTexture(snapshot_);
}

void InGameMenu::Paint() {
  if (first_paint_) {
    // Set first available index as default.
    int selection = -1;
    do {
      ++selection;
      if (selection < 0)
        selection = static_cast<int>(MenuItem::kMax) - 1;
      else
        selection %= static_cast<int>(MenuItem::kMax);
    } while (hide_menus_.find(selection) != hide_menus_.end());
    current_selection_ = static_cast<MenuItem>(selection);

    first_paint_ = false;
  }

  // Draw background
  ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
  ImVec2 window_pos = ImGui::GetWindowPos();
  ImVec2 window_size = ImGui::GetWindowSize();
  bg_draw_list->AddRectFilled(window_pos,
                              ImVec2(window_pos.x + window_size.x + 1,
                                     window_pos.y + window_size.y + 1),
                              IM_COL32(0, 0, 0, 196));

  // Elements
  const int kCenterX = window_size.x / 2;
  // Draw a new vertical line in the middle.
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(window_pos.x + kCenterX, 0),
      ImVec2(window_pos.x + kCenterX, window_pos.y + window_size.y),
      IM_COL32_WHITE);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 20));
  ScopedFont font(FontType::kDefault2x);

  // Draw main menu
  const char* kMenuItems[] = {
      "Continue", "Load State", "Save State", "Options", "Back To Main",
  };
  int menu_tops[static_cast<int>(MenuItem::kMax)];
  constexpr int kMargin = 10;
  int min_menu_x = INT_MAX;
  int font_height = 0;
  const int kMenuY = ImGui::GetCursorPos().y;
  int current_selection = 0;
  for (const char* item : kMenuItems) {
    if (hide_menus_.find(current_selection++) != hide_menus_.end())
      continue;

    ImVec2 text_size = font.GetFont()->CalcTextSizeA(font.GetFont()->FontSize,
                                                     FLT_MAX, FLT_MAX, item);
    font_height = text_size.y;
    int x = kCenterX - kMargin - text_size.x;
    if (min_menu_x < x)
      min_menu_x = x;
    ImGui::Dummy(text_size);
  }

  ImVec2 current_cursor = ImGui::GetCursorPos();
  ImVec2 menu_size =
      ImVec2(kCenterX - kMargin - min_menu_x, current_cursor.y - kMenuY);
  ImGui::SetCursorPosY((window_size.y - menu_size.y) / 2);

  current_selection = 0;
  for (const char* item : kMenuItems) {
    if (hide_menus_.find(current_selection) != hide_menus_.end()) {
      ++current_selection;
      continue;
    }

    menu_tops[current_selection] = ImGui::GetCursorPosY();
    ImVec2 text_size = font.GetFont()->CalcTextSizeA(font.GetFont()->FontSize,
                                                     FLT_MAX, FLT_MAX, item);
    ImGui::SetCursorPosX(kCenterX - kMargin - text_size.x);
    if (current_selection == static_cast<int>(current_selection_))
      ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s", item);
    else
      ImGui::Text("%s", item);

    ++current_selection;
  }

  // Draw save & load thumbnail
  const int kThumbnailWidth =
      Canvas::kNESFrameDefaultWidth / 3 * main_window_->window_scale();
  const int kThumbnailHeight =
      Canvas::kNESFrameDefaultHeight / 3 * main_window_->window_scale();

  if (current_selection_ == MenuItem::kSaveState ||
      current_selection_ == MenuItem::kLoadState) {
    SDL_Rect right_side_rect{kCenterX, 0,
                             static_cast<int>(window_size.x / 2 + 1),
                             static_cast<int>(window_size.y + 1)};
    ImVec2 thumbnail_pos(
        right_side_rect.x + (right_side_rect.w - kThumbnailWidth) / 2,
        right_side_rect.y + (right_side_rect.h - kThumbnailHeight) / 2);
    ImGui::SetCursorPos(thumbnail_pos);
    ImVec2 p0(thumbnail_pos.x, thumbnail_pos.y);
    ImVec2 p1(thumbnail_pos.x + kThumbnailWidth,
              thumbnail_pos.y + kThumbnailHeight);
    ImGui::GetWindowDrawList()->AddRect(
        ImVec2(window_pos.x + p0.x, window_pos.y + p0.y),
        ImVec2(window_pos.x + p1.x, window_pos.y + p1.y), IM_COL32_WHITE);
    // When the state is saved, NotifyThumbnailChanged() should be invoked. It
    // will create a snapshot texture.
    if (!runtime_data_->saved_state_thumbnail.empty()) {
      SDL_assert(snapshot_);
      ImGui::Image(snapshot_, ImVec2(kThumbnailWidth, kThumbnailHeight));
    } else {
      constexpr char kNoStateStr[] = "No State.";
      ImVec2 text_size = font.GetFont()->CalcTextSizeA(
          font.GetFont()->FontSize, FLT_MAX, FLT_MAX, kNoStateStr);
      ImGui::SetCursorPos(ImVec2(p0.x + (p1.x - p0.x - text_size.x) / 2,
                                 p0.y + (p1.y - p0.y - text_size.y) / 2));
      ImGui::Text("%s", kNoStateStr);
    }
  }
  ImGui::PopStyleVar();

  // Draw settings
  if (current_selection_ == MenuItem::kOptions) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 40));
    const char* kSettingsItems[] = {"Volume", "Window Size"};
    int settings_tops[static_cast<int>(SettingsItem::kMax)];
    const int kSettingsY = ImGui::GetCursorPos().y;
    for (const char* item : kSettingsItems) {
      ImVec2 text_size = font.GetFont()->CalcTextSizeA(font.GetFont()->FontSize,
                                                       FLT_MAX, FLT_MAX, item);
      font_height = text_size.y;
      ImGui::Dummy(text_size);
    }

    const char* kWindowSizes[] = {"Small", "Normal", "Large", "Fullscreen"};
    int window_scaling = static_cast<int>(main_window_->window_scale());
    SDL_assert(window_scaling > 1 && window_scaling <= 4);
    const char* kSizeStr = main_window_->is_fullscreen()
                               ? kWindowSizes[3]
                               : kWindowSizes[window_scaling - 2];
    ImVec2 window_text_size = font.GetFont()->CalcTextSizeA(
        font.GetFont()->FontSize, FLT_MAX, FLT_MAX, kSizeStr);
    ImGui::Dummy(window_text_size);

    current_cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPosY((window_size.y - (current_cursor.y - kSettingsY)) / 2);
    current_selection = 0;
    constexpr int kSettingsItemCount =
        sizeof(kSettingsItems) / sizeof(*kSettingsItems);
    for (int i = 0; i < kSettingsItemCount; ++i) {
      const char* item = kSettingsItems[i];
      if (i == kSettingsItemCount - 1)
        ImGui::PopStyleVar();
      settings_tops[current_selection] = ImGui::GetCursorPosY();
      ImVec2 text_size = font.GetFont()->CalcTextSizeA(font.GetFont()->FontSize,
                                                       FLT_MAX, FLT_MAX, item);
      ImGui::SetCursorPosX(kCenterX + kMargin);
      ImGui::Text("%s", item);
      ++current_selection;
    }

    ImGui::SetCursorPosX(kCenterX + kMargin +
                         (kCenterX - window_text_size.x) / 2);
    ImGui::Text("%s", kSizeStr);

    // Draw volume bar
    constexpr int kVolumeBarHeight = 20;
    constexpr int kVolumeBarSpacing = 10;
    constexpr int kPromptHeight = kVolumeBarHeight;
    constexpr int kPromptWidth = kPromptHeight * .8f;
    ImVec2 p0(
        window_pos.x + kCenterX + kMargin + kPromptWidth + kMargin,
        window_pos.y + settings_tops[0] + font_height + kVolumeBarSpacing);
    ImVec2 p1(window_pos.x + window_size.x - kMargin,
              window_pos.y + settings_tops[0] + font_height +
                  kVolumeBarSpacing + kVolumeBarHeight);
    ImGui::GetWindowDrawList()->AddRect(p0, p1, IM_COL32_WHITE);

    float volume = runtime_data_->emulator->GetVolume();
    const int kInnerBarWidth = (p1.x - p0.x) - 2;
    ImVec2 inner_p0(p0.x + 1, p0.y + 1);
    ImVec2 inner_p1(p0.x + 1 + (kInnerBarWidth * volume), p1.y - 1);
    ImGui::GetWindowDrawList()->AddRectFilled(inner_p0, inner_p1,
                                              IM_COL32_WHITE);

    if (settings_entered_) {
      if (current_setting_ == SettingsItem::kVolume) {
        ImGui::GetWindowDrawList()->AddTriangleFilled(
            ImVec2(p0.x - kPromptWidth - kVolumeBarSpacing, p0.y),
            ImVec2(p0.x - kPromptWidth - kVolumeBarSpacing,
                   p0.y + kPromptHeight),
            ImVec2(p0.x - kVolumeBarSpacing, p0.y + kPromptHeight / 2),
            IM_COL32_WHITE);
      } else if (current_setting_ == SettingsItem::kWindowSize) {
        ImVec2 scaling_triangle_p0(
            window_pos.x + kCenterX + kMargin + kPromptWidth + kMargin,
            window_pos.y + settings_tops[1] + font_height + kVolumeBarSpacing);

        if (window_scaling <= 2) {
          ImGui::GetWindowDrawList()->AddTriangle(
              ImVec2(scaling_triangle_p0.x - kPromptWidth - kVolumeBarSpacing,
                     scaling_triangle_p0.y + kPromptHeight / 2),
              ImVec2(scaling_triangle_p0.x - kVolumeBarSpacing,
                     scaling_triangle_p0.y),
              ImVec2(scaling_triangle_p0.x - kVolumeBarSpacing,
                     scaling_triangle_p0.y + kPromptHeight),
              IM_COL32_WHITE);
        } else {
          ImGui::GetWindowDrawList()->AddTriangleFilled(
              ImVec2(scaling_triangle_p0.x - kPromptWidth - kVolumeBarSpacing,
                     scaling_triangle_p0.y + kPromptHeight / 2),
              ImVec2(scaling_triangle_p0.x - kVolumeBarSpacing,
                     scaling_triangle_p0.y),
              ImVec2(scaling_triangle_p0.x - kVolumeBarSpacing,
                     scaling_triangle_p0.y + kPromptHeight),
              IM_COL32_WHITE);
        }

        if (!main_window_->is_fullscreen()) {
          ImGui::GetWindowDrawList()->AddTriangleFilled(
              ImVec2(window_pos.x + window_size.x - kMargin - kPromptWidth,
                     scaling_triangle_p0.y),
              ImVec2(window_pos.x + window_size.x - kMargin - kPromptWidth,
                     scaling_triangle_p0.y + kPromptHeight),
              ImVec2(window_pos.x + window_size.x - kMargin,
                     scaling_triangle_p0.y + kPromptHeight / 2),
              IM_COL32_WHITE);
        } else {
          ImGui::GetWindowDrawList()->AddTriangle(
              ImVec2(window_pos.x + window_size.x - kMargin - kPromptWidth,
                     scaling_triangle_p0.y),
              ImVec2(window_pos.x + window_size.x - kMargin - kPromptWidth,
                     scaling_triangle_p0.y + kPromptHeight),
              ImVec2(window_pos.x + window_size.x - kMargin,
                     scaling_triangle_p0.y + kPromptHeight / 2),
              IM_COL32_WHITE);
        }
      }
    }
  }

  // Draw selection
  constexpr int kSelectionPadding = 3;
  current_selection = static_cast<int>(current_selection_);
  ImVec2 selection_rect_pt0(0, menu_tops[current_selection]);
  ImVec2 selection_rect_pt1(kCenterX - 1,
                            menu_tops[current_selection] + font_height);
  bg_draw_list->AddRectFilled(
      ImVec2(window_pos.x + selection_rect_pt0.x,
             window_pos.y + selection_rect_pt0.y - kSelectionPadding),
      ImVec2(window_pos.x + selection_rect_pt1.x,
             window_pos.y + selection_rect_pt1.y + kSelectionPadding),
      IM_COL32_WHITE);
}

bool InGameMenu::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvents(event, nullptr);
}

bool InGameMenu::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvents(nullptr, event);
}

bool InGameMenu::OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) {
  return HandleInputEvents(nullptr, nullptr);
}

bool InGameMenu::HandleInputEvents(SDL_KeyboardEvent* k,
                                   SDL_ControllerButtonEvent* c) {
  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kUp, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
    PlayEffect(audio_resources::AudioID::kSelect);
    MoveSelection(true);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kDown, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
    PlayEffect(audio_resources::AudioID::kSelect);
    MoveSelection(false);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kA, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_A) {
    if (current_selection_ == MenuItem::kOptions) {
      PlayEffect(audio_resources::AudioID::kSelect);
      settings_entered_ = true;
    } else {
      PlayEffect(audio_resources::AudioID::kStart);
      menu_callback_.Run(current_selection_);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kB, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_B) {
    PlayEffect(audio_resources::AudioID::kBack);
    if (settings_entered_)
      settings_entered_ = false;
    else
      menu_callback_.Run(MenuItem::kContinue);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kLeft, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
    if (settings_entered_)
      settings_callback_.Run(current_setting_, true);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    if (settings_entered_)
      settings_callback_.Run(current_setting_, false);
    return true;
  }

  return false;
}

void InGameMenu::MoveSelection(bool up) {
  if (!settings_entered_) {
    int selection = static_cast<int>(current_selection_);
    do {
      selection = up ? selection - 1 : selection + 1;
      if (selection < 0)
        selection = static_cast<int>(MenuItem::kMax) - 1;
      else
        selection %= static_cast<int>(MenuItem::kMax);
    } while (hide_menus_.find(selection) != hide_menus_.end());
    current_selection_ = static_cast<MenuItem>(selection);
  } else {
    int selection = static_cast<int>(current_setting_);
    selection = up ? selection - 1 : selection + 1;
    if (selection < 0)
      selection = static_cast<int>(SettingsItem::kMax) - 1;
    else
      selection %= static_cast<int>(SettingsItem::kMax);
    current_setting_ = static_cast<SettingsItem>(selection);
  }
}

void InGameMenu::Close() {
  set_visible(false);
}

void InGameMenu::Show() {
  set_visible(true);
}

void InGameMenu::NotifyThumbnailChanged() {
  if (!runtime_data_->saved_state.empty()) {
    SDL_assert(!runtime_data_->saved_state_thumbnail.empty());
    if (!snapshot_) {
      snapshot_ = SDL_CreateTexture(
          window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
          SDL_TEXTUREACCESS_STREAMING, Canvas::kNESFrameDefaultWidth,
          Canvas::kNESFrameDefaultHeight);
    }
    SDL_UpdateTexture(
        snapshot_, nullptr, runtime_data_->saved_state_thumbnail.data(),
        Canvas::kNESFrameDefaultWidth *
            sizeof(runtime_data_->saved_state_thumbnail.data()[0]));
  }
}

void InGameMenu::HideMenu(int index) {
  hide_menus_.insert(index);
}