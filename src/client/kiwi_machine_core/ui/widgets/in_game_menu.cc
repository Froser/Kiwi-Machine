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

#include <imgui.h>
#include <climits>

#include "build/kiwi_defines.h"
#include "ui/application.h"
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

  loading_widget_ = std::make_unique<LoadingWidget>(main_window);
}

InGameMenu::~InGameMenu() {
  if (snapshot_)
    SDL_DestroyTexture(snapshot_);
}

void InGameMenu::SetFirstSelection() {
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
}

void InGameMenu::Paint() {
  if (first_paint_) {
    SetFirstSelection();
    RequestCurrentThumbnail();

    first_paint_ = false;
  }

#if KIWI_MOBILE
  CleanUpSettingsItemRects();
#endif

  // Draw background
  ImDrawList* bg_draw_list = ImGui::GetBackgroundDrawList();
  ImVec2 window_pos = ImGui::GetWindowPos();
  ImVec2 window_size = ImGui::GetWindowSize();
  bg_draw_list->AddRectFilled(window_pos,
                              ImVec2(window_pos.x + window_size.x + 1,
                                     window_pos.y + window_size.y + 1),
                              IM_COL32(0, 0, 0, 196));

  // Elements
  // Triangle prompt size:
  const int kCenterX = window_size.x / 2;
  // Draw a new vertical line in the middle.
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(window_pos.x + kCenterX, 0),
      ImVec2(window_pos.x + kCenterX, window_pos.y + window_size.y),
      IM_COL32_WHITE);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 20));
  ScopedFont font(main_window_->window_scale() > 3.f
                      ? FontType::kDefault3x
                      : (main_window_->window_scale() > 2.f
                             ? FontType::kDefault2x
                             : FontType::kDefault));

  // Draw main menu
  const char* kMenuItems[] = {
      "Continue", "Load Auto Save", "Load State",   "Save State",
      "Options",  "Reset Game",     "Back To Main",
  };
  int menu_tops[static_cast<int>(MenuItem::kMax)];
  int menu_font_size = font.GetFont()->FontSize;

  constexpr int kMargin = 10;
  int min_menu_x = INT_MAX;
  const int kMenuY = ImGui::GetCursorPos().y;
  int current_selection = 0;
  for (const char* item : kMenuItems) {
    if (hide_menus_.find(current_selection++) != hide_menus_.end())
      continue;

    ImVec2 text_size = ImGui::CalcTextSize(item);
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
    ImVec2 text_size = ImGui::CalcTextSize(item);
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
      current_selection_ == MenuItem::kLoadAutoSave ||
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

    // Draw triangle, to switch states.
    // Center of the snapshot rect
    const int kSnapshotPromptHeight = 7 * main_window_->window_scale();
    const int kSnapshotPromptWidth = kSnapshotPromptHeight * .8f;

    constexpr int kSnapshotPromptSpacing = 10;
    const int kSnapshotPromptY =
        p0.y + (p1.y - p0.y - kSnapshotPromptHeight) / 2;

    bool left_enabled = true;
    bool right_enabled = true;

    if (current_selection_ == MenuItem::kLoadAutoSave) {
      if (which_autosave_state_slot_ == 0) {
        right_enabled = false;
      }

      const kiwi::nes::RomData* rom_data =
          runtime_data_->emulator->GetRomData();
      SDL_assert(rom_data);
      if (which_autosave_state_slot_ == current_auto_states_count_)
        left_enabled = false;
    }

#if KIWI_MOBILE
    AddRectForSettingsItem(
        static_cast<int>(current_selection_),
        SDL_Rect{static_cast<int>(window_pos.x + p0.x - kSnapshotPromptSpacing -
                                  kSnapshotPromptWidth),
                 static_cast<int>(window_pos.y + kSnapshotPromptY +
                                  kSnapshotPromptHeight / 2),
                 kSnapshotPromptWidth, kSnapshotPromptHeight},
        SDL_Rect{static_cast<int>(window_pos.x + p1.x + kSnapshotPromptSpacing),
                 static_cast<int>(window_pos.y + kSnapshotPromptY),
                 kSnapshotPromptWidth, kSnapshotPromptHeight});
#endif

    // Left
    if (left_enabled) {
      ImGui::GetWindowDrawList()->AddTriangleFilled(
          ImVec2(window_pos.x + p0.x - kSnapshotPromptSpacing -
                     kSnapshotPromptWidth,
                 window_pos.y + kSnapshotPromptY + kSnapshotPromptHeight / 2),
          ImVec2(window_pos.x + p0.x - kSnapshotPromptSpacing,
                 window_pos.y + kSnapshotPromptY),
          ImVec2(window_pos.x + p0.x - kSnapshotPromptSpacing,
                 window_pos.y + kSnapshotPromptY + kSnapshotPromptHeight),
          IM_COL32_WHITE);
    } else {
      ImGui::GetWindowDrawList()->AddTriangle(
          ImVec2(window_pos.x + p0.x - kSnapshotPromptSpacing -
                     kSnapshotPromptWidth,
                 window_pos.y + kSnapshotPromptY + kSnapshotPromptHeight / 2),
          ImVec2(window_pos.x + p0.x - kSnapshotPromptSpacing,
                 window_pos.y + kSnapshotPromptY),
          ImVec2(window_pos.x + p0.x - kSnapshotPromptSpacing,
                 window_pos.y + kSnapshotPromptY + kSnapshotPromptHeight),
          IM_COL32_WHITE);
    }

    // Right
    if (right_enabled) {
      ImGui::GetWindowDrawList()->AddTriangleFilled(
          ImVec2(window_pos.x + p1.x + kSnapshotPromptSpacing,
                 window_pos.y + kSnapshotPromptY),
          ImVec2(window_pos.x + p1.x + kSnapshotPromptSpacing,
                 window_pos.y + kSnapshotPromptY + kSnapshotPromptHeight),
          ImVec2(window_pos.x + p1.x + kSnapshotPromptSpacing +
                     kSnapshotPromptWidth,
                 window_pos.y + kSnapshotPromptY + kSnapshotPromptHeight / 2),
          IM_COL32_WHITE);
    } else {
      ImGui::GetWindowDrawList()->AddTriangle(
          ImVec2(window_pos.x + p1.x + kSnapshotPromptSpacing,
                 window_pos.y + kSnapshotPromptY),
          ImVec2(window_pos.x + p1.x + kSnapshotPromptSpacing,
                 window_pos.y + kSnapshotPromptY + kSnapshotPromptHeight),
          ImVec2(window_pos.x + p1.x + kSnapshotPromptSpacing +
                     kSnapshotPromptWidth,
                 window_pos.y + kSnapshotPromptY + kSnapshotPromptHeight / 2),
          IM_COL32_WHITE);
    }

    // When the state is saved, RequestCurrentThumbnail() should be invoked. It
    // will create a snapshot texture.
    if (is_loading_snapshot_) {
      SDL_Rect spin_aabb = loading_widget_->CalculateCircleAABB(nullptr);
      ImVec2 spin_size(spin_aabb.w, spin_aabb.h);
      SDL_Rect loading_bounds{
          static_cast<int>(p0.x + (p1.x - p0.x - spin_size.x) / 2),
          static_cast<int>(p0.y + (p1.y - p0.y - spin_size.y) / 2), 20, 20};
      loading_widget_->set_spinning_bounds(loading_bounds);
      loading_widget_->Paint();
    } else if (currently_has_snapshot_) {
      SDL_assert(snapshot_);
      ImGui::Image(snapshot_, ImVec2(kThumbnailWidth, kThumbnailHeight));
    } else {
      constexpr char kNoStateStr[] = "No State.";
      ImVec2 text_size = ImGui::CalcTextSize(kNoStateStr);
      ImGui::SetCursorPos(ImVec2(p0.x + (p1.x - p0.x - text_size.x) / 2,
                                 p0.y + (p1.y - p0.y - text_size.y) / 2));
      ImGui::Text("%s", kNoStateStr);
    }

    // Draw text
    {
      std::string state_slot_str;
      if (current_selection_ == MenuItem::kLoadAutoSave) {
        if (state_timestamp_) {
          // Show the title as auto save's date.
          // Format timestamp.
          time_t time(state_timestamp_);
          state_slot_str = asctime(localtime(&time));
        }
      } else {
        // Slot number in string starts at 1.
        state_slot_str = "Slot " + kiwi::base::NumberToString(which_state_ + 1);
      }

      {
        ScopedFont slot_font(FontType::kDefault);
        ImVec2 text_size = ImGui::CalcTextSize(state_slot_str.c_str());
        constexpr int kSlotSpacing = 10;
        ImGui::SetCursorPos(ImVec2(p0.x + (p1.x - p0.x - text_size.x) / 2,
                                   p1.y + kSnapshotPromptSpacing));
        ImGui::Text("%s", state_slot_str.c_str());
      }
    }
  }

  ImGui::PopStyleVar();

  // Draw settings
  if (current_selection_ == MenuItem::kOptions) {
    ScopedFont sys_font(main_window_->window_scale() > 3.f
                            ? FontType::kSystemDefault3x
                            : (main_window_->window_scale() > 2.f
                                   ? FontType::kSystemDefault2x
                                   : FontType::kSystemDefault));

    int window_scaling_for_settings =
        static_cast<int>(main_window_->window_scale());
    if (window_scaling_for_settings < 2)
      window_scaling_for_settings = 2;
    else if (window_scaling_for_settings > 4)
      window_scaling_for_settings = 4;

    // Beginning of calculating settings' position
    // Including main settings, volume bar, window sizes, joysticks, etc.
    const char* kSettingsItems[] = {"Volume", "Window Size", "Joysticks"};
    const int kSettingsY = ImGui::GetCursorPos().y;
    for (const char* item : kSettingsItems) {
      ImVec2 text_size = ImGui::CalcTextSize(item);
      ImGui::Dummy(text_size);
    }

    // Volume bar
    const int kVolumeBarHeight = 7 * window_scaling_for_settings;
    const int kVolumeBarSpacing = 3 * window_scaling_for_settings;
    ImGui::Dummy(ImVec2(1, kVolumeBarHeight));
    // Window size
    {
      ScopedFont size_font(main_window_->window_scale() > 3.f
                               ? FontType::kDefault3x
                               : (main_window_->window_scale() > 2.f
                                      ? FontType::kDefault2x
                                      : FontType::kDefault));
      ImGui::Dummy(ImVec2(1, ImGui::GetFontSize()));
    }
    // Joysticks
    {
      ScopedFont joy_font(main_window_->is_fullscreen() ? FontType::kDefault2x
                                                        : FontType::kDefault);
      ImGui::Dummy(ImVec2(1, ImGui::GetFontSize()));
    }

    current_cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPosY((window_size.y - (current_cursor.y - kSettingsY)) / 2);
    // End of calculating settings' position

    current_selection = 0;
    constexpr int kSettingsItemCount =
        sizeof(kSettingsItems) / sizeof(*kSettingsItems);
    for (int i = 0; i < kSettingsItemCount; ++i) {
      const char* item = kSettingsItems[i];
      ImVec2 text_size = ImGui::CalcTextSize(item);
      ImGui::SetCursorPosX(kCenterX + kMargin);
      ImGui::Text("%s", item);

      switch (i) {
        case 0: {  // Volume:
#if !KIWI_MOBILE
          // PC application has a volume bar
          float prompt_height = kVolumeBarHeight;
          float prompt_width = prompt_height * .8f;
          ImVec2 p0(window_pos.x + kCenterX + kMargin + prompt_width + kMargin,
                    window_pos.y + ImGui::GetCursorPosY());
          ImVec2 p1(window_pos.x + window_size.x - kMargin,
                    window_pos.y + ImGui::GetCursorPosY() + kVolumeBarHeight);

          ImGui::GetWindowDrawList()->AddRect(p0, p1, IM_COL32_WHITE);
          ImGui::Dummy(ImVec2(p1.x - p0.x, p1.y - p0.y));

          float volume = runtime_data_->emulator->GetVolume();
          const int kInnerBarWidth = (p1.x - p0.x) - 2;
          ImVec2 inner_p0(p0.x + 1, p0.y + 1);
          ImVec2 inner_p1(p0.x + 1 + (kInnerBarWidth * volume), p1.y - 1);
          ImGui::GetWindowDrawList()->AddRectFilled(inner_p0, inner_p1,
                                                    IM_COL32_WHITE);
          if (settings_entered_ && current_setting_ == SettingsItem::kVolume) {
            ImGui::GetWindowDrawList()->AddTriangleFilled(
                ImVec2(p0.x - prompt_width - kVolumeBarSpacing, p0.y),
                ImVec2(p0.x - prompt_width - kVolumeBarSpacing,
                       p0.y + prompt_height),
                ImVec2(p0.x - kVolumeBarSpacing, p0.y + prompt_height / 2),
                IM_COL32_WHITE);
          }
#else
          // There are only 2 options on mobiles: On or Off.
          ScopedFont size_font(main_window_->window_scale() > 3.f
                                   ? FontType::kDefault3x
                                   : (main_window_->window_scale() > 2.f
                                          ? FontType::kDefault2x
                                          : FontType::kDefault));
          float prompt_height = ImGui::GetFontSize();
          float prompt_width = prompt_height * .8f;
          float volume = runtime_data_->emulator->GetVolume();
          const char* kVolumeStr = volume > 0 ? "On" : "Off";

          ImVec2 window_text_size = ImGui::CalcTextSize(kVolumeStr);
          ImGui::SetCursorPosX(kCenterX + kMargin +
                               (kCenterX - window_text_size.x) / 2);
          int text_y = ImGui::GetCursorPosY();
          ImGui::Text("%s", kVolumeStr);

          settings_positions_[SettingsItem::kVolume] =
              SDL_Rect{kCenterX, text_y, kCenterX,
                       static_cast<int>(ImGui::GetCursorPosY() - text_y)};

          if (settings_entered_ && current_setting_ == SettingsItem::kVolume) {
            bool has_no_left = volume <= 0.f;
            bool has_right = has_no_left;

            ImVec2 triangle_p0(
                window_pos.x + kCenterX + kMargin + prompt_width + kMargin,
                window_pos.y + text_y);

            AddRectForSettingsItem(
                0,
                SDL_Rect{static_cast<int>(triangle_p0.x - prompt_width -
                                          window_pos.x),
                         static_cast<int>(triangle_p0.y),
                         static_cast<int>(prompt_width),
                         static_cast<int>(prompt_height)},
                SDL_Rect{
                    static_cast<int>(window_size.x - kMargin - prompt_width),
                    static_cast<int>(triangle_p0.y),
                    static_cast<int>(prompt_width),
                    static_cast<int>(prompt_height)});

            if (has_no_left) {
              ImGui::GetWindowDrawList()->AddTriangle(
                  ImVec2(triangle_p0.x - prompt_width - kVolumeBarSpacing,
                         triangle_p0.y + prompt_height / 2),
                  ImVec2(triangle_p0.x - kVolumeBarSpacing, triangle_p0.y),
                  ImVec2(triangle_p0.x - kVolumeBarSpacing,
                         triangle_p0.y + prompt_height),
                  IM_COL32_WHITE);
            } else {
              ImGui::GetWindowDrawList()->AddTriangleFilled(
                  ImVec2(triangle_p0.x - prompt_width - kVolumeBarSpacing,
                         triangle_p0.y + prompt_height / 2),
                  ImVec2(triangle_p0.x - kVolumeBarSpacing, triangle_p0.y),
                  ImVec2(triangle_p0.x - kVolumeBarSpacing,
                         triangle_p0.y + prompt_height),
                  IM_COL32_WHITE);
            }

            if (has_right) {
              ImGui::GetWindowDrawList()->AddTriangleFilled(
                  ImVec2(window_pos.x + window_size.x - kMargin - prompt_width,
                         triangle_p0.y),
                  ImVec2(window_pos.x + window_size.x - kMargin - prompt_width,
                         triangle_p0.y + prompt_height),
                  ImVec2(window_pos.x + window_size.x - kMargin,
                         triangle_p0.y + prompt_height / 2),
                  IM_COL32_WHITE);
            } else {
              ImGui::GetWindowDrawList()->AddTriangle(
                  ImVec2(window_pos.x + window_size.x - kMargin - prompt_width,
                         triangle_p0.y),
                  ImVec2(window_pos.x + window_size.x - kMargin - prompt_width,
                         triangle_p0.y + prompt_height),
                  ImVec2(window_pos.x + window_size.x - kMargin,
                         triangle_p0.y + prompt_height / 2),
                  IM_COL32_WHITE);
            }
          }
#endif
        } break;
        case 1: {  // Window size:
          ScopedFont size_font(main_window_->window_scale() > 3.f
                                   ? FontType::kDefault3x
                                   : (main_window_->window_scale() > 2.f
                                          ? FontType::kDefault2x
                                          : FontType::kDefault));
#if !KIWI_MOBILE
          const char* kWindowSizes[] = {"Small", "Normal", "Large",
                                        "Fullscreen"};
          const char* kSizeStr =
              main_window_->is_fullscreen()
                  ? kWindowSizes[3]
                  : kWindowSizes[window_scaling_for_settings - 2];
#else
          // Android and mobile apps only has two modes: streching mode and
          // non-streching mode.
          const char* kSizeStr =
              main_window_->is_stretch_mode() ? "Stretch" : "Original";
#endif

          ImVec2 window_text_size = ImGui::CalcTextSize(kSizeStr);
          ImGui::SetCursorPosX(kCenterX + kMargin +
                               (kCenterX - window_text_size.x) / 2);
          int text_y = ImGui::GetCursorPosY();
          ImGui::Text("%s", kSizeStr);

#if KIWI_MOBILE
          settings_positions_[SettingsItem::kWindowSize] =
              SDL_Rect{kCenterX, text_y, kCenterX,
                       static_cast<int>(ImGui::GetCursorPosY() - text_y)};
#endif

          if (settings_entered_ &&
              current_setting_ == SettingsItem::kWindowSize) {
            float prompt_height = ImGui::GetFontSize();
            float prompt_width = prompt_height * .8f;

            ImVec2 triangle_p0(
                window_pos.x + kCenterX + kMargin + prompt_width + kMargin,
                window_pos.y + text_y);

#if !KIWI_MOBILE
            bool has_no_left = window_scaling_for_settings <= 2;
            bool has_right = !main_window_->is_fullscreen();
#else
            bool has_no_left = !main_window_->is_stretch_mode();
            bool has_right = has_no_left;

            AddRectForSettingsItem(
                1,
                SDL_Rect{static_cast<int>(triangle_p0.x - prompt_width -
                                          window_pos.x),
                         static_cast<int>(triangle_p0.y),
                         static_cast<int>(prompt_width),
                         static_cast<int>(prompt_height)},
                SDL_Rect{
                    static_cast<int>(window_size.x - kMargin - prompt_width),
                    static_cast<int>(triangle_p0.y),
                    static_cast<int>(prompt_width),
                    static_cast<int>(prompt_height)});
#endif

            if (has_no_left) {
              ImGui::GetWindowDrawList()->AddTriangle(
                  ImVec2(triangle_p0.x - prompt_width - kVolumeBarSpacing,
                         triangle_p0.y + prompt_height / 2),
                  ImVec2(triangle_p0.x - kVolumeBarSpacing, triangle_p0.y),
                  ImVec2(triangle_p0.x - kVolumeBarSpacing,
                         triangle_p0.y + prompt_height),
                  IM_COL32_WHITE);
            } else {
              ImGui::GetWindowDrawList()->AddTriangleFilled(
                  ImVec2(triangle_p0.x - prompt_width - kVolumeBarSpacing,
                         triangle_p0.y + prompt_height / 2),
                  ImVec2(triangle_p0.x - kVolumeBarSpacing, triangle_p0.y),
                  ImVec2(triangle_p0.x - kVolumeBarSpacing,
                         triangle_p0.y + prompt_height),
                  IM_COL32_WHITE);
            }

            if (has_right) {
              ImGui::GetWindowDrawList()->AddTriangleFilled(
                  ImVec2(window_pos.x + window_size.x - kMargin - prompt_width,
                         triangle_p0.y),
                  ImVec2(window_pos.x + window_size.x - kMargin - prompt_width,
                         triangle_p0.y + prompt_height),
                  ImVec2(window_pos.x + window_size.x - kMargin,
                         triangle_p0.y + prompt_height / 2),
                  IM_COL32_WHITE);
            } else {
              ImGui::GetWindowDrawList()->AddTriangle(
                  ImVec2(window_pos.x + window_size.x - kMargin - prompt_width,
                         triangle_p0.y),
                  ImVec2(window_pos.x + window_size.x - kMargin - prompt_width,
                         triangle_p0.y + prompt_height),
                  ImVec2(window_pos.x + window_size.x - kMargin,
                         triangle_p0.y + prompt_height / 2),
                  IM_COL32_WHITE);
            }
          }
        } break;
        case 2: {  // Joysticks
          for (int j = 0; j < 2; ++j) {
            const int kJoyDescSpacing = 3 * main_window_->window_scale();
#if !KIWI_MOBILE
            ScopedFont joy_font(main_window_->is_fullscreen()
                                    ? FontType::kDefault2x
                                    : FontType::kDefault);
#else
            ScopedFont joy_font(FontType::kDefault2x);
#endif
            std::string joyname =
                std::string("P") + kiwi::base::NumberToString(j + 1) +
                std::string(": ") +
                (runtime_data_->joystick_mappings[j].which
                     ? (SDL_GameControllerName(
                           reinterpret_cast<SDL_GameController*>(
                               runtime_data_->joystick_mappings[j].which)))
                     : "None");
            ImVec2 window_text_size = ImGui::CalcTextSize(joyname.c_str());
            ImGui::SetCursorPosX(kCenterX + kMargin +
                                 (kCenterX - window_text_size.x) / 2);
            int text_y = ImGui::GetCursorPosY();
            ImGui::Text("%s", joyname.c_str());
            float prompt_height = ImGui::GetFontSize();
            float prompt_width = prompt_height * .8f;

#if KIWI_MOBILE
            settings_positions_[static_cast<SettingsItem>(
                j + static_cast<int>(SettingsItem::kJoyP1))] =
                SDL_Rect{kCenterX, text_y, kCenterX,
                         static_cast<int>(ImGui::GetCursorPosY() - text_y)};
#endif

            if (settings_entered_ &&
                    (j == 0 && current_setting_ == SettingsItem::kJoyP1) ||
                (j == 1 && current_setting_ == SettingsItem::kJoyP2)) {
              ImVec2 triangle_p0(
                  window_pos.x + kCenterX + kMargin + prompt_width + kMargin,
                  window_pos.y + text_y);

#if KIWI_MOBILE
              AddRectForSettingsItem(
                  static_cast<int>(current_setting_),
                  SDL_Rect{static_cast<int>(triangle_p0.x - prompt_width -
                                            kVolumeBarSpacing),
                           static_cast<int>(triangle_p0.y),
                           static_cast<int>(prompt_width),
                           static_cast<int>(prompt_height)},
                  SDL_Rect{static_cast<int>(window_pos.x + window_size.x -
                                            kMargin - prompt_width),
                           static_cast<int>(triangle_p0.y),
                           static_cast<int>(prompt_width),
                           static_cast<int>(prompt_height)});
#endif

              bool has_left = false, has_right = false;
              auto controllers = GetControllerList();
              auto target_iter =
                  std::find(controllers.begin(), controllers.end(),
                            (reinterpret_cast<SDL_GameController*>(
                                runtime_data_->joystick_mappings[j].which)));
              has_left = (*target_iter != nullptr);
              has_right = target_iter != controllers.end() &&
                          target_iter < controllers.end() - 1;
              if (!has_left) {
                ImGui::GetWindowDrawList()->AddTriangle(
                    ImVec2(triangle_p0.x - prompt_width - kVolumeBarSpacing,
                           triangle_p0.y + prompt_height / 2),
                    ImVec2(triangle_p0.x - kVolumeBarSpacing, triangle_p0.y),
                    ImVec2(triangle_p0.x - kVolumeBarSpacing,
                           triangle_p0.y + prompt_height),
                    IM_COL32_WHITE);
              } else {
                ImGui::GetWindowDrawList()->AddTriangleFilled(
                    ImVec2(triangle_p0.x - prompt_width - kVolumeBarSpacing,
                           triangle_p0.y + prompt_height / 2),
                    ImVec2(triangle_p0.x - kVolumeBarSpacing, triangle_p0.y),
                    ImVec2(triangle_p0.x - kVolumeBarSpacing,
                           triangle_p0.y + prompt_height),
                    IM_COL32_WHITE);
              }

              if (has_right) {
                ImGui::GetWindowDrawList()->AddTriangleFilled(
                    ImVec2(
                        window_pos.x + window_size.x - kMargin - prompt_width,
                        triangle_p0.y),
                    ImVec2(
                        window_pos.x + window_size.x - kMargin - prompt_width,
                        triangle_p0.y + prompt_height),
                    ImVec2(window_pos.x + window_size.x - kMargin,
                           triangle_p0.y + prompt_height / 2),
                    IM_COL32_WHITE);
              } else {
                ImGui::GetWindowDrawList()->AddTriangle(
                    ImVec2(
                        window_pos.x + window_size.x - kMargin - prompt_width,
                        triangle_p0.y),
                    ImVec2(
                        window_pos.x + window_size.x - kMargin - prompt_width,
                        triangle_p0.y + prompt_height),
                    ImVec2(window_pos.x + window_size.x - kMargin,
                           triangle_p0.y + prompt_height / 2),
                    IM_COL32_WHITE);
              }
            }
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + kJoyDescSpacing);
          }

        } break;
        default:
          break;
      }

      ++current_selection;
    }
  }

  // Draw selection
  constexpr int kSelectionPadding = 3;
  current_selection = static_cast<int>(current_selection_);
  ImVec2 selection_rect_pt0(0, menu_tops[current_selection]);
  ImVec2 selection_rect_pt1(kCenterX - 1, menu_tops[current_selection]);
  bg_draw_list->AddRectFilled(
      ImVec2(window_pos.x + selection_rect_pt0.x,
             window_pos.y + selection_rect_pt0.y - kSelectionPadding),
      ImVec2(window_pos.x + selection_rect_pt1.x,
             window_pos.y + selection_rect_pt1.y + kSelectionPadding +
                 menu_font_size),
      IM_COL32_WHITE);

#if KIWI_MOBILE
  // Register menu position to response finger touch events.
  // The responding area is exactly the 'draw-selection area'.
  if (menu_positions_.empty()) {
    int i = 0;
    for (const char* item : kMenuItems) {
      if (hide_menus_.find(i) != hide_menus_.end()) {
        // The current menu 'i' is hidden.
        ++i;
      } else {
        int menu_top = menu_tops[i];
        menu_positions_[static_cast<MenuItem>(i)] =
            SDL_Rect{0, menu_top - kSelectionPadding, kCenterX - 1,
                     2 * kSelectionPadding + menu_font_size};
        ++i;
      }
    }
  }
#endif
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
    HandleMenuItemForCurrentSelection();
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kB, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_B) {
    PlayEffect(audio_resources::AudioID::kBack);
    if (settings_entered_)
      settings_entered_ = false;
    else
      menu_callback_.Run(MenuItem::kContinue, 0);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kLeft, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
    HandleSettingsItemForCurrentSelection(true);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    HandleSettingsItemForCurrentSelection(false);
    return true;
  }

  return false;
}

void InGameMenu::HandleMenuItemForCurrentSelection() {
  if (current_selection_ == MenuItem::kOptions) {
    PlayEffect(audio_resources::AudioID::kSelect);
    settings_entered_ = true;
  } else {
    PlayEffect(audio_resources::AudioID::kStart);
    if (current_selection_ == MenuItem::kLoadState ||
        current_selection_ == MenuItem::kSaveState) {
      // Saving or loading states will pass a parameter to show which state
      // to be saved.
      menu_callback_.Run(current_selection_, which_state_);
    } else if (current_selection_ == MenuItem::kLoadAutoSave) {
      menu_callback_.Run(current_selection_, state_timestamp_);
    } else {
      menu_callback_.Run(current_selection_, 0);
    }
  }
}

void InGameMenu::HandleSettingsItemForCurrentSelection(bool go_left) {
  if (go_left) {
    if (settings_entered_) {
      settings_callback_.Run(current_setting_, true);
    } else {
      if (current_selection_ == MenuItem::kSaveState ||
          current_selection_ == MenuItem::kLoadState) {
        --which_state_;
        if (which_state_ < 0)
          which_state_ = NESRuntime::Data::MaxSaveStates - 1;
        RequestCurrentThumbnail();
      } else if (current_selection_ == MenuItem::kLoadAutoSave) {
        const kiwi::nes::RomData* rom_data =
            runtime_data_->emulator->GetRomData();
        SDL_assert(rom_data);
        RequestCurrentSaveStatesCount();
        if (which_autosave_state_slot_ < current_auto_states_count_) {
          ++which_autosave_state_slot_;
          RequestCurrentThumbnail();
        }
      }
    }
  } else {
    if (settings_entered_) {
      settings_callback_.Run(current_setting_, false);
    } else {
      if (current_selection_ == MenuItem::kSaveState ||
          current_selection_ == MenuItem::kLoadState) {
        which_state_ = (which_state_ + 1) % NESRuntime::Data::MaxSaveStates;
        RequestCurrentThumbnail();
      } else if (current_selection_ == MenuItem::kLoadAutoSave) {
        if (which_autosave_state_slot_ > 0) {
          --which_autosave_state_slot_;
          RequestCurrentThumbnail();
        }
      }
    }
  }
}

void InGameMenu::MoveSelection(bool up) {
  if (!settings_entered_) {
    int selection = static_cast<int>(current_selection_);
    MenuItem last_selection = static_cast<MenuItem>(selection);
    do {
      selection = up ? selection - 1 : selection + 1;
      if (selection < 0)
        selection = static_cast<int>(MenuItem::kMax) - 1;
      else
        selection %= static_cast<int>(MenuItem::kMax);
    } while (hide_menus_.find(selection) != hide_menus_.end());
    current_selection_ = static_cast<MenuItem>(selection);

    if (current_selection_ == MenuItem::kLoadAutoSave) {
      which_autosave_state_slot_ = 0;
      state_timestamp_ = 0;
      RequestCurrentSaveStatesCount();
      RequestCurrentThumbnail();
    } else if ((current_selection_ == MenuItem::kSaveState ||
                current_selection_ == MenuItem::kLoadState) &&
               (last_selection != MenuItem::kSaveState &&
                last_selection != MenuItem::kLoadState)) {
      // When enter load/save state, request thumbnail.
      RequestCurrentThumbnail();
    }
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
  SetFirstSelection();
  set_visible(true);
}

void InGameMenu::RequestCurrentThumbnail() {
  currently_has_snapshot_ = false;
  is_loading_snapshot_ = true;
  const kiwi::nes::RomData* rom_data = runtime_data_->emulator->GetRomData();
  // Settings menu also use this class, but no ROM is loaded.
  if (rom_data) {
    if (current_selection_ == MenuItem::kLoadAutoSave) {
      runtime_data_->GetAutoSavedState(
          rom_data->crc, which_autosave_state_slot_,
          kiwi::base::BindOnce(&InGameMenu::OnGotState,
                               kiwi::base::Unretained(this)));
    } else {
      runtime_data_->GetState(
          rom_data->crc, which_state_,
          kiwi::base::BindOnce(&InGameMenu::OnGotState,
                               kiwi::base::Unretained(this)));
    }
  }
}

void InGameMenu::RequestCurrentSaveStatesCount() {
  const kiwi::nes::RomData* rom_data = runtime_data_->emulator->GetRomData();
  if (rom_data) {
    runtime_data_->GetAutoSavedStatesCount(
        rom_data->crc, kiwi::base::BindOnce(
                           [](InGameMenu* this_menu, int count) {
                             this_menu->current_auto_states_count_ = count;
                           },
                           this));
  }
}

void InGameMenu::OnGotState(const NESRuntime::Data::StateResult& state_result) {
  is_loading_snapshot_ = false;
  if (state_result.success && !state_result.state_data.empty()) {
    currently_has_snapshot_ = true;
    SDL_assert(!state_result.thumbnail_data.empty());
    if (!snapshot_) {
      snapshot_ = SDL_CreateTexture(
          window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
          SDL_TEXTUREACCESS_STREAMING, Canvas::kNESFrameDefaultWidth,
          Canvas::kNESFrameDefaultHeight);
    }

    constexpr int kColorComponents = 4;
    SDL_UpdateTexture(snapshot_, nullptr, state_result.thumbnail_data.data(),
                      Canvas::kNESFrameDefaultWidth * kColorComponents *
                          sizeof(state_result.thumbnail_data.data()[0]));

    // state_timestamp_ will only be used in showing auto-saved state's title.
    state_timestamp_ = state_result.slot_or_timestamp;
  } else {
    currently_has_snapshot_ = false;
  }
}

void InGameMenu::HideMenu(int index) {
  hide_menus_.insert(index);
}