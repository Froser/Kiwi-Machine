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
#include "ui/styles.h"
#include "ui/widgets/canvas.h"
#include "utility/audio_effects.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"
#include "utility/math.h"

namespace {

void DrawTrianglePrompt(bool has_left,
                        bool has_right,
                        const Triangle& left,
                        const Triangle& right) {
  if (!has_left) {
    ImGui::GetWindowDrawList()->AddTriangle(left.point[0], left.point[1],
                                            left.point[2], IM_COL32_WHITE);
  } else {
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        left.point[0], left.point[1], left.point[2], IM_COL32_WHITE);
  }

  if (!has_right) {
    ImGui::GetWindowDrawList()->AddTriangle(right.point[0], right.point[1],
                                            right.point[2], IM_COL32_WHITE);
  } else {
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        right.point[0], right.point[1], right.point[2], IM_COL32_WHITE);
  }
}

}  // namespace

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
  current_menu_ = static_cast<MenuItem>(selection);
}

void InGameMenu::EnterSettings(InGameMenu::MenuItem item) {
  // Changes status during rendering is forbidden, because it may cause crashes
  // on Android and such platforms. Posts it as a task.
  kiwi::base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, kiwi::base::BindOnce(
                     [](InGameMenu* t, InGameMenu::MenuItem item) {
                       SDL_assert(!t->is_rendering_);
                       if (t->settings_entered_)
                         t->settings_entered_ = false;
                       if (t->current_menu_ != item)
                         t->MoveMenuItemTo(item);
                       else
                         t->HandleMenuItemForCurrentSelection();
                     },
                     kiwi::base::Unretained(this), item));
}

void InGameMenu::EnterSettings(InGameMenu::SettingsItem item) {
  // Changes status during rendering is forbidden, because it may cause crashes
  // on Android and such platforms. Posts it as a task.
  kiwi::base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      kiwi::base::BindOnce(
          [](InGameMenu* t, InGameMenu::SettingsItem item) {
            SDL_assert(!t->is_rendering_);
            // If the setting hasn't been entered, enter the setting first.
            // If current setting item has already been selected, propagates the
            // event to its prompt.
            if (t->current_setting_ != item || !t->settings_entered_) {
              t->settings_entered_ = true;
              t->current_setting_ = item;
            }
          },
          kiwi::base::Unretained(this), item));
}

void InGameMenu::HandleSettingsPrompts(SettingsItem item, bool go_left) {
  // Changes status during rendering is forbidden, because it may cause crashes
  // on Android and such platforms. Posts it as a task.
  kiwi::base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      kiwi::base::BindOnce(
          [](InGameMenu* t, InGameMenu::SettingsItem item, bool go_left) {
            SDL_assert(!t->is_rendering_);
            t->HandleSettingsPromptsInternal(item, go_left);
          },
          kiwi::base::Unretained(this), item, go_left));
}

void InGameMenu::HandleOtherPrompts(bool go_left) {
  // Changes status during rendering is forbidden, because it may cause crashes
  // on Android and such platforms. Posts it as a task.
  kiwi::base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, kiwi::base::BindOnce(
                     [](InGameMenu* t, bool go_left) {
                       SDL_assert(!t->is_rendering_);
                       t->HandleOtherPromptsInternal(go_left);
                     },
                     kiwi::base::Unretained(this), go_left));
}

void InGameMenu::HandleVolume(float percentage) {
  // Changes status during rendering is forbidden, because it may cause crashes
  // on Android and such platforms. Posts it as a task.
  kiwi::base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, kiwi::base::BindOnce(
                     [](InGameMenu* t, float percentage) {
                       SDL_assert(!t->is_rendering_);
                       SDL_assert(t->current_menu_ == MenuItem::kOptions);
                       t->settings_callback_.Run(SettingsItem::kVolume,
                                                 percentage);
                     },
                     kiwi::base::Unretained(this), percentage));
}

void InGameMenu::Paint() {
  is_rendering_ = true;

  if (first_paint_) {
    SetFirstSelection();
    RequestCurrentThumbnail();

    first_paint_ = false;
  }

  LayoutImmediateContext context = PreLayoutImmediate();
  DrawBackgroundImmediate(context);
  DrawMenuItemsImmediate(context);
  DrawSelectionImmediate(context);

  is_rendering_ = false;
}

bool InGameMenu::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvent(event, nullptr);
}

bool InGameMenu::OnMouseReleased(SDL_MouseButtonEvent* event) {
  if (event->button == SDL_BUTTON_RIGHT) {
    PlayEffect(audio_resources::AudioID::kBack);
    menu_callback_.Run(MenuItem::kToGameSelection, 0);
    return true;
  }

  return false;
}

bool InGameMenu::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvent(nullptr, event);
}

bool InGameMenu::OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) {
  return HandleInputEvent(nullptr, nullptr);
}

bool InGameMenu::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  // InGameMenu will stop propagation, preventing touch event passing to the
  // widget below it, such as FullscreenMask widget in MainWindow.
  return true;
}

void InGameMenu::OnWindowPreRender() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
}

void InGameMenu::OnWindowPostRender() {
  ImGui::PopStyleVar(2);
}

bool InGameMenu::HandleInputEvent(SDL_KeyboardEvent* k,
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
      c && c->button == SDL_CONTROLLER_BUTTON_X) {
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
    if (current_menu_ == MenuItem::kOptions) {
      if (settings_entered_)
        HandleSettingsPrompts(current_setting_, true);
    } else {
      HandleOtherPrompts(true);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    if (current_menu_ == MenuItem::kOptions) {
      if (settings_entered_)
        HandleSettingsPrompts(current_setting_, false);
    } else {
      HandleOtherPrompts(false);
    }
    return true;
  }

  return false;
}

void InGameMenu::HandleMenuItemForCurrentSelection() {
  if (current_menu_ == MenuItem::kOptions) {
    PlayEffect(audio_resources::AudioID::kSelect);
    settings_entered_ = true;
  } else {
    if (current_menu_ == MenuItem::kLoadState ||
        current_menu_ == MenuItem::kSaveState) {
      // Saving or loading states will pass a parameter to show which state
      // to be saved.
      PlayEffect(audio_resources::AudioID::kStart);
      menu_callback_.Run(current_menu_, which_state_);
    } else if (current_menu_ == MenuItem::kLoadAutoSave) {
      PlayEffect(audio_resources::AudioID::kStart);
      menu_callback_.Run(current_menu_, state_timestamp_);
    } else {
      if (current_menu_ == MenuItem::kToGameSelection)
        PlayEffect(audio_resources::AudioID::kBack);
      else
        PlayEffect(audio_resources::AudioID::kStart);

      menu_callback_.Run(current_menu_, 0);
    }
  }
}

void InGameMenu::HandleSettingsPromptsInternal(InGameMenu::SettingsItem item,
                                               bool go_left) {
  SDL_assert(!is_rendering_);
  SDL_assert(current_menu_ == MenuItem::kOptions);
  current_setting_ = item;
  if (settings_entered_) {
    settings_callback_.Run(current_setting_, go_left);
  }
}

void InGameMenu::HandleOtherPromptsInternal(bool go_left) {
  SDL_assert(!is_rendering_);
  SDL_assert(current_menu_ != MenuItem::kOptions);
  if (go_left) {
    if (current_menu_ == MenuItem::kSaveState ||
        current_menu_ == MenuItem::kLoadState) {
      --which_state_;
      if (which_state_ < 0)
        which_state_ = NESRuntime::Data::MaxSaveStates - 1;
      RequestCurrentThumbnail();
    } else if (current_menu_ == MenuItem::kLoadAutoSave) {
      const kiwi::nes::RomData* rom_data =
          runtime_data_->emulator->GetRomData();
      SDL_assert(rom_data);
      RequestCurrentSaveStatesCount();
      if (which_autosave_state_slot_ < current_auto_states_count_) {
        ++which_autosave_state_slot_;
        RequestCurrentThumbnail();
      }
    }
  } else {
    if (current_menu_ == MenuItem::kSaveState ||
        current_menu_ == MenuItem::kLoadState) {
      which_state_ = (which_state_ + 1) % NESRuntime::Data::MaxSaveStates;
      RequestCurrentThumbnail();
    } else if (current_menu_ == MenuItem::kLoadAutoSave) {
      if (which_autosave_state_slot_ > 0) {
        --which_autosave_state_slot_;
        RequestCurrentThumbnail();
      }
    }
  }
}

void InGameMenu::MoveSelection(bool up) {
  if (!settings_entered_) {
    int selection = static_cast<int>(current_menu_);
    do {
      selection = up ? selection - 1 : selection + 1;
      if (selection < 0)
        selection = static_cast<int>(MenuItem::kMax) - 1;
      else
        selection %= static_cast<int>(MenuItem::kMax);
    } while (hide_menus_.find(selection) != hide_menus_.end());

    MoveMenuItemTo(static_cast<MenuItem>(selection));
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

void InGameMenu::MoveMenuItemTo(MenuItem item) {
  if (!settings_entered_) {
    MenuItem last_selection = current_menu_;
    current_menu_ = item;
    if (current_menu_ == MenuItem::kLoadAutoSave) {
      which_autosave_state_slot_ = 0;
      state_timestamp_ = 0;
      RequestCurrentSaveStatesCount();
      RequestCurrentThumbnail();
    } else if ((current_menu_ == MenuItem::kSaveState ||
                current_menu_ == MenuItem::kLoadState) &&
               (last_selection != MenuItem::kSaveState &&
                last_selection != MenuItem::kLoadState)) {
      // When enter load/save state, request thumbnail.
      RequestCurrentThumbnail();
    }
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
    if (current_menu_ == MenuItem::kLoadAutoSave) {
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

bool InGameMenu::IsItemBeingPressed(const SDL_Rect& item_rect,
                                    const LayoutImmediateContext& context) {
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    ImVec2 mouse_pos = ImGui::GetMousePos();
    SDL_Rect global_item_rect = MapToWindow(item_rect);
    // MapToWindow calculates window's title bar's height, so we minus it
    // here.
    global_item_rect.y -= context.title_menu_height;
    if (Contains(global_item_rect, mouse_pos.x, mouse_pos.y)) {
      return true;
    }
  }

  return false;
}

InGameMenu::LayoutImmediateContext InGameMenu::PreLayoutImmediate() {
  LayoutImmediateContext context;
  SDL_Rect safe_area_insets = main_window_->GetSafeAreaInsets();
  context.window_pos = ImGui::GetWindowPos();
  context.window_size = ImGui::GetWindowSize();

  context.font_size =
      styles::in_game_menu::GetPreferredFontSize(main_window_->window_scale());

  // Calculates the safe area on iPhone or other devices.
  context.window_pos.x += safe_area_insets.x;
  context.window_pos.y += safe_area_insets.y;
  context.window_size.x -= safe_area_insets.x + safe_area_insets.w;
  context.window_size.y -= safe_area_insets.y + safe_area_insets.h;

  // Menu items
  context.menu_items = {
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_CONTINUE).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_LOAD_AUTO_SAVE)
          .c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_LOAD_STATE).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_SAVE_STATE).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_OPTIONS).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_RESET_GAME).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_BACK_TO_MAIN)
          .c_str(),
  };

  // Options
  context.options_items = {
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_VOLUME).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_WINDOW_SIZE)
          .c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_JOYSTICKS).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_LANGUAGE).c_str(),
  };

  // Handlers' order must match drawing order
  context.options_handlers = {
      &InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_Volume,
      &InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_WindowSize,
      &InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_Joysticks,
      &InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_Languages,
  };

  // On PC apps, it may has a debug menu.
  const_cast<int&>(context.title_menu_height) = context.window_pos.y;

  const_cast<int&>(context.window_center_x) =
      context.window_pos.x + context.window_size.x / 2;

  return context;
}

void InGameMenu::DrawBackgroundImmediate(LayoutImmediateContext& context) {
  ImVec2 window_fullscreen_pos = ImGui::GetWindowPos();
  ImVec2 window_fullscreen_size = ImGui::GetWindowSize();
  ImGui::GetWindowDrawList()->AddRectFilled(
      window_fullscreen_pos,
      ImVec2(window_fullscreen_pos.x + window_fullscreen_size.x + 1,
             window_fullscreen_pos.y + window_fullscreen_size.y + 1),
      IM_COL32(0, 0, 0, 196));

  // Draw a new vertical line in the middle.
  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(context.window_center_x, 0),
      ImVec2(context.window_center_x,
             window_fullscreen_pos.y + window_fullscreen_size.y),
      IM_COL32_WHITE);
}

void InGameMenu::DrawMenuItemsImmediate(LayoutImmediateContext& context) {
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                      ImVec2(0, styles::in_game_menu::GetOptionsSpacing()));

  DrawMenuItemsImmediate_DrawMenuItems(context);
  ImGui::PopStyleVar();
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Layout(
    LayoutImmediateContext& context) {
  ScopedFont font = GetPreferredFont(context.font_size, context.menu_items[0]);
  context.menu_font_size = font.GetFont()->FontSize;

  int min_menu_x = INT_MAX;
  const int kMenuY = ImGui::GetCursorPosY();
  int current_selection = 0;
  for (const char* item : context.menu_items) {
    if (hide_menus_.find(current_selection++) != hide_menus_.end())
      continue;

    ImVec2 text_size = ImGui::CalcTextSize(item);
    int x = context.window_center_x - kMenuItemMargin - text_size.x;
    if (min_menu_x < x)
      min_menu_x = x;
    ImGui::Dummy(text_size);
  }

  ImVec2 current_cursor = ImGui::GetCursorPos();
  ImVec2 menu_size =
      ImVec2(context.window_center_x - kMenuItemMargin - min_menu_x,
             current_cursor.y - kMenuY);
  ImGui::SetCursorPosY(context.window_pos.y +
                       (context.window_size.y - menu_size.y) / 2);
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems(
    LayoutImmediateContext& context) {
  constexpr int kSelectionPadding = 3;
  DrawMenuItemsImmediate_DrawMenuItems_Layout(context);

  ScopedFont font = GetPreferredFont(context.font_size, context.menu_items[0]);
  int current_selection = 0;
  for (const char* item : context.menu_items) {
    if (hide_menus_.find(current_selection) != hide_menus_.end()) {
      ++current_selection;
      continue;
    }

    context.settings_menu_item_rects[current_selection] =
        SDL_Rect{0,
                 static_cast<int>(ImGui::GetCursorPosY()) +
                     context.title_menu_height - kSelectionPadding,
                 context.window_center_x - 1,
                 kSelectionPadding * 2 + context.menu_font_size};

    ImVec2 text_size = ImGui::CalcTextSize(item);
    ImGui::SetCursorPosX(context.window_center_x - kMenuItemMargin -
                         text_size.x);
    if (current_selection == static_cast<int>(current_menu_)) {
      context.selection_menu_item_position = ImGui::GetCursorPos();
      context.selection_menu_item_text = item;
      ImGui::Dummy(text_size);
    } else {
      ImGui::Text("%s", item);
    }

    if (IsItemBeingPressed(context.settings_menu_item_rects[current_selection],
                           context)) {
      EnterSettings(static_cast<InGameMenu::MenuItem>(current_selection));
    }

    ++current_selection;
  }

  DrawMenuItemsImmediate_DrawMenuItems_SaveLoad(context);
  DrawMenuItemsImmediate_DrawMenuItems_Options(context);
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_SaveLoad(
    LayoutImmediateContext& context) {
  // Draw save & load thumbnail
  const int kThumbnailWidth = styles::in_game_menu::GetSnapshotThumbnailWidth(
      main_window_->IsLandscape(), main_window_->window_scale());
  const int kThumbnailHeight = styles::in_game_menu::GetSnapshotThumbnailHeight(
      main_window_->IsLandscape(), main_window_->window_scale());

  if (current_menu_ == MenuItem::kSaveState ||
      current_menu_ == MenuItem::kLoadAutoSave ||
      current_menu_ == MenuItem::kLoadState) {
    SDL_Rect right_side_rect{context.window_center_x,
                             static_cast<int>(context.window_pos.y),
                             static_cast<int>(context.window_size.x / 2 + 1),
                             static_cast<int>(context.window_size.y + 1)};
    ImVec2 thumbnail_pos(
        right_side_rect.x + (right_side_rect.w - kThumbnailWidth) / 2,
        right_side_rect.y + (right_side_rect.h - kThumbnailHeight) / 2);
    ImGui::SetCursorPos(thumbnail_pos);
    ImVec2 p0(thumbnail_pos.x, thumbnail_pos.y + context.title_menu_height);
    ImVec2 p1(thumbnail_pos.x + kThumbnailWidth,
              thumbnail_pos.y + kThumbnailHeight + context.title_menu_height);
    ImGui::GetWindowDrawList()->AddRect(ImVec2(p0.x, p0.y), ImVec2(p1.x, p1.y),
                                        IM_COL32_WHITE);
    SDL_Rect thumbnail_rect = {static_cast<int>(p0.x), static_cast<int>(p0.y),
                               static_cast<int>(p1.x - p0.x),
                               static_cast<int>(p1.y - p0.y)};
    if (IsItemBeingPressed(thumbnail_rect, context)) {
      EnterSettings(current_menu_);
    }

    // Draw triangle, to switch states.
    // Center of the snapshot rect
    const int kSnapshotPromptHeight =
        styles::in_game_menu::GetSnapshotPromptHeight(
            main_window_->window_scale());
    const int kSnapshotPromptWidth = kSnapshotPromptHeight * .8f;

    constexpr int kSnapshotPromptSpacing = 10;
    const int kSnapshotPromptY =
        p0.y + (p1.y - p0.y - kSnapshotPromptHeight) / 2;

    bool left_enabled = true;
    bool right_enabled = true;

    if (current_menu_ == MenuItem::kLoadAutoSave) {
      if (which_autosave_state_slot_ == 0) {
        right_enabled = false;
      }

      const kiwi::nes::RomData* rom_data =
          runtime_data_->emulator->GetRomData();
      SDL_assert(rom_data);
      if (which_autosave_state_slot_ == current_auto_states_count_)
        left_enabled = false;
    }

    Triangle prompt_left{
        ImVec2(p0.x - kSnapshotPromptSpacing - kSnapshotPromptWidth,
               kSnapshotPromptY + kSnapshotPromptHeight / 2),
        ImVec2(p0.x - kSnapshotPromptSpacing, kSnapshotPromptY),
        ImVec2(p0.x - kSnapshotPromptSpacing,
               kSnapshotPromptY + kSnapshotPromptHeight)};
    Triangle prompt_right{
        ImVec2(p1.x + kSnapshotPromptSpacing, kSnapshotPromptY),
        ImVec2(p1.x + kSnapshotPromptSpacing,
               kSnapshotPromptY + kSnapshotPromptHeight),
        ImVec2(p1.x + kSnapshotPromptSpacing + kSnapshotPromptWidth,
               kSnapshotPromptY + kSnapshotPromptHeight / 2)};
    DrawTrianglePrompt(left_enabled, right_enabled, prompt_left, prompt_right);

    if (IsItemBeingPressed(prompt_left.BoundingBox(), context))
      HandleOtherPrompts(true);
    else if (IsItemBeingPressed(prompt_right.BoundingBox(), context))
      HandleOtherPrompts(false);

    // When the state is saved, RequestCurrentThumbnail() should be invoked.
    // It will create a snapshot texture.
    if (is_loading_snapshot_) {
      SDL_Rect spin_aabb = loading_widget_->CalculateCircleAABB(nullptr);
      ImVec2 spin_size(spin_aabb.w, spin_aabb.h);
      SDL_Rect loading_bounds{
          static_cast<int>(p0.x + (p1.x - p0.x - spin_size.x) / 2 +
                           context.title_menu_height),
          static_cast<int>(p0.y + (p1.y - p0.y - spin_size.y) / 2 +
                           context.title_menu_height),
          20, 20};
      loading_widget_->set_spinning_bounds(loading_bounds);
      loading_widget_->Paint();
    } else if (currently_has_snapshot_) {
      SDL_assert(snapshot_);
      ImGui::Image(reinterpret_cast<ImTextureID>(snapshot_),
                   ImVec2(kThumbnailWidth, kThumbnailHeight));

    } else {
#if KIWI_IOS
      std::unique_ptr<ScopedFont> no_state_font;
      if (!main_window_->IsLandscape()) {
        no_state_font = std::make_unique<ScopedFont>(FontType::kSystemDefault);
      }
#endif
      const char* kNoStateStr =
          GetLocalizedString(string_resources::IDR_IN_GAME_MENU_NO_STATE)
              .c_str();
      ImVec2 text_size = ImGui::CalcTextSize(kNoStateStr);
      ImGui::SetCursorPos(ImVec2(p0.x + (p1.x - p0.x - text_size.x) / 2,
                                 p0.y + (p1.y - p0.y - text_size.y) / 2));
      ImGui::Text("%s", kNoStateStr);
    }

    // Draw text
    {
      std::string state_slot_str;
      if (current_menu_ == MenuItem::kLoadAutoSave) {
        if (state_timestamp_) {
          // Show the title as auto save's date.
          // Format timestamp.
          time_t time(state_timestamp_);
          state_slot_str = asctime(localtime(&time));
        }
      } else {
        // Slot number in string starts at 1.
        state_slot_str =
            GetLocalizedString(string_resources::IDR_IN_GAME_MENU_SLOT) +
            kiwi::base::NumberToString(which_state_ + 1);
      }

      {
        ScopedFont slot_font =
            GetPreferredFont(context.font_size, state_slot_str.c_str());
        ImVec2 text_size = ImGui::CalcTextSize(state_slot_str.c_str());
        ImGui::SetCursorPos(ImVec2(p0.x + (p1.x - p0.x - text_size.x) / 2,
                                   p1.y + kSnapshotPromptSpacing));
        ImGui::Text("%s", state_slot_str.c_str());
      }
    }
  }
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options(
    LayoutImmediateContext& context) {
  // Draw settings
  if (current_menu_ == MenuItem::kOptions) {
    DrawMenuItemsImmediate_DrawMenuItems_OptionsLayout(context);
    DrawMenuItemsImmediate_DrawMenuItems_Options_PaintEachOption(context);
  }
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_OptionsLayout(
    LayoutImmediateContext& context) {
  ScopedFont option_font = GetPreferredFont(
      context.font_size, context.options_items[0], FontType::kSystemDefault);

  int window_scaling_for_options =
      static_cast<int>(main_window_->window_scale());
  if (main_window_->is_fullscreen())
    window_scaling_for_options = kMaxScaling;
  if (window_scaling_for_options < 2)
    window_scaling_for_options = 2;
  else if (window_scaling_for_options > 4)
    window_scaling_for_options = 4;

  const int kSettingsY = ImGui::GetCursorPosY();
  for (const char* item : context.options_items) {
    ImVec2 text_size = ImGui::CalcTextSize(item);
    ImGui::Dummy(text_size);
  }

  // Beginning of calculating settings' position
  // Including main settings, volume bar, window sizes, joysticks, etc.
  // Volume bar
  context.volume_bar_height = 7 * window_scaling_for_options;
  context.options_items_spacing = 3 * window_scaling_for_options;
  context.window_scaling_for_options = window_scaling_for_options;
  ImGui::Dummy(ImVec2(1, context.volume_bar_height));

  // Window size
  {
    ScopedFont scoped_font(
        GetPreferredFontType(context.font_size, context.options_items[1]));
    ImGui::Dummy(ImVec2(1, ImGui::GetFontSize()));
  }
  // Joysticks
  {
    ScopedFont scoped_font(
        GetPreferredFontType(context.font_size, context.options_items[2]));
    ImGui::Dummy(ImVec2(1, ImGui::GetFontSize()));
  }
  // Languages
  {
    ScopedFont scoped_font(
        GetPreferredFontType(context.font_size, context.options_items[3]));
    ImGui::Dummy(ImVec2(1, ImGui::GetFontSize()));
  }

  ImVec2 current_cursor = ImGui::GetCursorPos();
  ImGui::SetCursorPosY((context.window_pos.y + context.window_size.y -
                        (current_cursor.y - kSettingsY)) /
                       2);
  // End of calculating settings' position
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_PaintEachOption(
    LayoutImmediateContext& context) {
  for (int i = 0; i < context.options_items.size(); ++i) {
    const char* item = context.options_items[i];
    ImVec2 text_size = ImGui::CalcTextSize(item);
    ImGui::SetCursorPosX(context.window_center_x + kMenuItemMargin);
    ImGui::Text("%s", item);
    std::invoke(context.options_handlers[i], this, context);
  }
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_Volume(
    LayoutImmediateContext& context) {
#if !KIWI_MOBILE
  // PC application has a volume bar
  float prompt_height = context.volume_bar_height;
  float prompt_width = prompt_height * .8f;
  ImVec2 p0(context.window_center_x + kMenuItemMargin + prompt_width +
                kMenuItemMargin,
            ImGui::GetCursorPosY() + context.title_menu_height);
  ImVec2 p1(context.window_size.x - kMenuItemMargin,
            ImGui::GetCursorPosY() + context.volume_bar_height +
                context.title_menu_height);

  ImGui::GetWindowDrawList()->AddRect(p0, p1, IM_COL32_WHITE);
  const SDL_Rect kVolumeMenuBounds{
      static_cast<int>(p0.x), static_cast<int>(p0.y),
      static_cast<int>(p1.x - p0.x), context.volume_bar_height};
  if (IsItemBeingPressed(kVolumeMenuBounds, context)) {
    EnterSettings(SettingsItem::kVolume);
  }

  const SDL_Rect kVolumeOperationBounds = {
      static_cast<int>(p0.x), static_cast<int>(p0.y),
      static_cast<int>(p1.x - p0.x), context.volume_bar_height};
  if (IsItemBeingPressed(kVolumeOperationBounds, context)) {
    ImVec2 mouse_pos = ImGui::GetMousePos();
    float percentage = (mouse_pos.x - kVolumeOperationBounds.x) /
                       static_cast<float>(kVolumeOperationBounds.w);
    HandleVolume(percentage);
  }

  ImGui::Dummy(ImVec2(p1.x - p0.x, p1.y - p0.y));

  float volume = runtime_data_->emulator->GetVolume();
  const int kInnerBarWidth = (p1.x - p0.x) - 2;
  ImVec2 inner_p0(p0.x + 1, p0.y + 1);
  ImVec2 inner_p1(p0.x + 1 + (kInnerBarWidth * volume), p1.y - 1);
  ImGui::GetWindowDrawList()->AddRectFilled(inner_p0, inner_p1, IM_COL32_WHITE);

  if (settings_entered_ && current_setting_ == SettingsItem::kVolume) {
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        ImVec2(p0.x - prompt_width - context.options_items_spacing, p0.y),
        ImVec2(p0.x - prompt_width - context.options_items_spacing,
               p0.y + prompt_height),
        ImVec2(p0.x - context.options_items_spacing, p0.y + prompt_height / 2),
        IM_COL32_WHITE);
  }
#else
  float volume = runtime_data_->emulator->GetVolume();
  const char* kVolumeStr =
      volume > 0
          ? GetLocalizedString(string_resources::IDR_IN_GAME_MENU_ON).c_str()
          : GetLocalizedString(string_resources::IDR_IN_GAME_MENU_OFF).c_str();

  // There are only 2 options on mobiles: On or Off.emulator->GetVolume();
  ScopedFont scoped_font(GetPreferredFontType(context.font_size, kVolumeStr));
  float prompt_height = ImGui::GetFontSize();
  float prompt_width = prompt_height * .8f;

  ImVec2 window_text_size = ImGui::CalcTextSize(kVolumeStr);
  ImGui::SetCursorPosX(context.window_center_x + kMenuItemMargin +
                       (context.window_size.x / 2 - window_text_size.x) / 2);
  int text_y = ImGui::GetCursorPosY();
  ImGui::Text("%s", kVolumeStr);

  if (IsItemBeingPressed(
          SDL_Rect{context.window_center_x, text_y, context.window_center_x,
                   static_cast<int>(ImGui::GetCursorPosY() - text_y)},
          context)) {
    EnterSettings(SettingsItem::kVolume);
  }

  if (settings_entered_ && current_setting_ == SettingsItem::kVolume) {
    bool has_no_left = volume <= 0.f;
    bool has_right = has_no_left;

    ImVec2 triangle_p0(context.window_center_x + kMenuItemMargin +
                           prompt_width + kMenuItemMargin,
                       text_y + context.title_menu_height);

    Triangle prompt_left{
        ImVec2(triangle_p0.x - prompt_width - context.options_items_spacing,
               triangle_p0.y + prompt_height / 2),
        ImVec2(triangle_p0.x - context.options_items_spacing, triangle_p0.y),
        ImVec2(triangle_p0.x - context.options_items_spacing,
               triangle_p0.y + prompt_height)};

    Triangle prompt_right{
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin -
                   prompt_width,
               triangle_p0.y),
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin -
                   prompt_width,
               triangle_p0.y + prompt_height),
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin,
               triangle_p0.y + prompt_height / 2),
    };

    AddRectForSettingsItemPrompt(SettingsItem::kVolume,
                                 prompt_left.BoundingBox(),
                                 prompt_right.BoundingBox(), context);
    DrawTrianglePrompt(!has_no_left, has_right, prompt_left, prompt_right);
  }
#endif
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_WindowSize(
    LayoutImmediateContext& context) {
#if !KIWI_MOBILE
  const char* kWindowSizes[] = {
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_SMALL).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_NORMAL).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_LARGE).c_str(),
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_FULLSCREEN)
          .c_str()};
  const char* kSizeStr =
      main_window_->is_fullscreen()
          ? kWindowSizes[3]
          : kWindowSizes[context.window_scaling_for_options - 2];
#else
  // Android and mobile apps only has two modes: streching mode and
  // non-streching mode.
  const char* kSizeStr =
      main_window_->is_stretch_mode()
          ? GetLocalizedString(string_resources::IDR_IN_GAME_MENU_STRETCH)
                .c_str()
          : GetLocalizedString(string_resources::IDR_IN_GAME_MENU_ORIGINAL)
                .c_str();
#endif

  ScopedFont scoped_font = GetPreferredFont(context.font_size, kSizeStr);
  ImVec2 window_text_size = ImGui::CalcTextSize(kSizeStr);
  ImGui::SetCursorPosX(context.window_center_x + kMenuItemMargin +
                       (context.window_size.x / 2 - window_text_size.x) / 2);
  int text_y = ImGui::GetCursorPosY();
  ImGui::Text("%s", kSizeStr);

  if (IsItemBeingPressed(
          SDL_Rect{context.window_center_x, text_y, context.window_center_x,
                   static_cast<int>(ImGui::GetCursorPosY() - text_y)},
          context)) {
    EnterSettings(SettingsItem::kWindowSize);
  }

  if (settings_entered_ && current_setting_ == SettingsItem::kWindowSize) {
    float prompt_height = ImGui::GetFontSize();
    float prompt_width = prompt_height * .8f;

    ImVec2 triangle_p0(context.window_center_x + kMenuItemMargin +
                           prompt_width + kMenuItemMargin,
                       text_y + context.title_menu_height);

#if !KIWI_MOBILE
    bool has_no_left = context.window_scaling_for_options <= 2;
#if !KIWI_WASM
    bool has_right = !main_window_->is_fullscreen();
#else
    // Disable window settings. It should be handled by <canvas>.
    has_no_left = true;
    bool has_right = false;
#endif
#else
    bool has_no_left = !main_window_->is_stretch_mode();
    bool has_right = has_no_left;
#endif

    Triangle prompt_left{
        ImVec2(triangle_p0.x - prompt_width - context.options_items_spacing,
               triangle_p0.y + prompt_height / 2),
        ImVec2(triangle_p0.x - context.options_items_spacing, triangle_p0.y),
        ImVec2(triangle_p0.x - context.options_items_spacing,
               triangle_p0.y + prompt_height),
    };
    Triangle prompt_right{
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin -
                   prompt_width,
               triangle_p0.y),
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin -
                   prompt_width,
               triangle_p0.y + prompt_height),
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin,
               triangle_p0.y + prompt_height / 2),
    };
    AddRectForSettingsItemPrompt(SettingsItem::kWindowSize,
                                 prompt_left.BoundingBox(),
                                 prompt_right.BoundingBox(), context);
    DrawTrianglePrompt(!has_no_left, has_right, prompt_left, prompt_right);
  }
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_Joysticks(
    LayoutImmediateContext& context) {
  constexpr int kPlayerStrings[] = {string_resources::IDR_IN_GAME_MENU_P1,
                                    string_resources::IDR_IN_GAME_MENU_P2};
  for (int j = 0; j < 2; ++j) {
    const int kJoyDescSpacing = 3 * main_window_->window_scale();
    const char* kStrNone =
        GetLocalizedString(string_resources::IDR_IN_GAME_MENU_NONE).c_str();
    ScopedFont joy_font = GetPreferredFont(context.font_size);
    std::string joyname =
        GetLocalizedString(kPlayerStrings[j]) +
        (runtime_data_->joystick_mappings[j].which
             ? (SDL_GameControllerName(reinterpret_cast<SDL_GameController*>(
                   runtime_data_->joystick_mappings[j].which)))
             : kStrNone);
    ImVec2 window_text_size = ImGui::CalcTextSize(joyname.c_str());
    ImGui::SetCursorPosX(context.window_center_x + kMenuItemMargin +
                         (context.window_size.x / 2 - window_text_size.x) / 2);
    int text_y = ImGui::GetCursorPosY();
    ImGui::Text("%s", joyname.c_str());
    float prompt_height = ImGui::GetFontSize();
    float prompt_width = prompt_height * .8f;

    if (IsItemBeingPressed(
            SDL_Rect{context.window_center_x, text_y, context.window_center_x,
                     static_cast<int>(ImGui::GetCursorPosY() - text_y)},
            context)) {
      EnterSettings(static_cast<SettingsItem>(
          j + static_cast<int>(SettingsItem::kJoyP1)));
    }

    if (settings_entered_ &&
            (j == 0 && current_setting_ == SettingsItem::kJoyP1) ||
        (j == 1 && current_setting_ == SettingsItem::kJoyP2)) {
      ImVec2 triangle_p0(context.window_center_x + kMenuItemMargin +
                             prompt_width + kMenuItemMargin,
                         text_y + context.title_menu_height);

      bool has_left = false, has_right = false;
      auto controllers = GetControllerList();
      auto target_iter =
          std::find(controllers.begin(), controllers.end(),
                    (reinterpret_cast<SDL_GameController*>(
                        runtime_data_->joystick_mappings[j].which)));
      has_left = (*target_iter != nullptr);
      has_right = target_iter != controllers.end() &&
                  target_iter < controllers.end() - 1;

      Triangle prompt_left{
          ImVec2(triangle_p0.x - prompt_width - context.options_items_spacing,
                 triangle_p0.y + prompt_height / 2),
          ImVec2(triangle_p0.x - context.options_items_spacing, triangle_p0.y),
          ImVec2(triangle_p0.x - context.options_items_spacing,
                 triangle_p0.y + prompt_height)};
      Triangle prompt_right{
          ImVec2(context.window_pos.x + context.window_size.x -
                     kMenuItemMargin - prompt_width,
                 triangle_p0.y),
          ImVec2(context.window_pos.x + context.window_size.x -
                     kMenuItemMargin - prompt_width,
                 triangle_p0.y + prompt_height),
          ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin,
                 triangle_p0.y + prompt_height / 2)};
      AddRectForSettingsItemPrompt(current_setting_, prompt_left.BoundingBox(),
                                   prompt_right.BoundingBox(), context);
      DrawTrianglePrompt(has_left, has_right, prompt_left, prompt_right);
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + kJoyDescSpacing);
  }
}

void InGameMenu::DrawMenuItemsImmediate_DrawMenuItems_Options_Languages(
    LayoutImmediateContext& context) {
  SupportedLanguage language = GetCurrentSupportedLanguage();
  const char* str_lang = nullptr;
  switch (language) {
#if !DISABLE_CHINESE_FONT
    case SupportedLanguage::kSimplifiedChinese:
      str_lang =
          GetLocalizedString(string_resources::IDR_IN_GAME_MENU_LANGUAGE_ZH)
              .c_str();
      break;
#endif
#if !DISABLE_JAPANESE_FONT
    case SupportedLanguage::kJapanese:
      str_lang =
          GetLocalizedString(string_resources::IDR_IN_GAME_MENU_LANGUAGE_JP)
              .c_str();
      break;
#endif
    case SupportedLanguage::kEnglish:
    default:
      str_lang =
          GetLocalizedString(string_resources::IDR_IN_GAME_MENU_LANGUAGE_EN)
              .c_str();
      break;
  }
  SDL_assert(str_lang);

  ScopedFont scoped_font = GetPreferredFont(context.font_size, str_lang);

  float prompt_height = ImGui::GetFontSize();
  float prompt_width = prompt_height * .8f;
  ImVec2 window_text_size = ImGui::CalcTextSize(str_lang);
  ImGui::SetCursorPosX(context.window_center_x + kMenuItemMargin +
                       (context.window_size.x / 2 - window_text_size.x) / 2);
  int text_y = ImGui::GetCursorPosY();
  ImGui::Text("%s", str_lang);

  if (IsItemBeingPressed(
          SDL_Rect{context.window_center_x, text_y, context.window_center_x,
                   static_cast<int>(ImGui::GetCursorPosY() - text_y)},
          context)) {
    EnterSettings(SettingsItem::kLanguage);
  }

  if (settings_entered_ && current_setting_ == SettingsItem::kLanguage) {
    ImVec2 triangle_p0(context.window_center_x + kMenuItemMargin +
                           prompt_width + kMenuItemMargin,
                       text_y + context.title_menu_height);
    constexpr bool kSupportChangeLanguage =
        static_cast<int>(SupportedLanguage::kMax) > 1;
    bool has_left = kSupportChangeLanguage, has_right = kSupportChangeLanguage;
    Triangle prompt_left{
        ImVec2(triangle_p0.x - prompt_width - context.options_items_spacing,
               triangle_p0.y + prompt_height / 2),
        ImVec2(triangle_p0.x - context.options_items_spacing, triangle_p0.y),
        ImVec2(triangle_p0.x - context.options_items_spacing,
               triangle_p0.y + prompt_height)};
    Triangle prompt_right{
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin -
                   prompt_width,
               triangle_p0.y),
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin -
                   prompt_width,
               triangle_p0.y + prompt_height),
        ImVec2(context.window_pos.x + context.window_size.x - kMenuItemMargin,
               triangle_p0.y + prompt_height / 2)};
    DrawTrianglePrompt(has_left, has_right, prompt_left, prompt_right);
    AddRectForSettingsItemPrompt(SettingsItem::kLanguage,
                                 prompt_left.BoundingBox(),
                                 prompt_right.BoundingBox(), context);
  }
}

void InGameMenu::DrawSelectionImmediate(LayoutImmediateContext& context) {
  // Draw selection
  int current_selection = static_cast<int>(current_menu_);
  const SDL_Rect& current_rect =
      context.settings_menu_item_rects[current_selection];
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(current_rect.x, current_rect.y),
      ImVec2(current_rect.x + current_rect.w, current_rect.y + current_rect.h),
      IM_COL32_WHITE);

  // Draw selection menu item after selection rect drawn
  {
    ScopedFont font =
        GetPreferredFont(context.font_size, context.menu_items[0]);
    ImVec2 pos_cache = ImGui::GetCursorPos();
    ImGui::SetCursorPos(context.selection_menu_item_position);
    ImGui::TextColored(ImVec4(0.f, 0.f, 0.f, 1.f), "%s",
                       context.selection_menu_item_text);
    ImGui::SetCursorPos(pos_cache);
  }
}

void InGameMenu::AddRectForSettingsItemPrompt(
    SettingsItem settings_index,
    const SDL_Rect& rect_for_left_prompt,
    const SDL_Rect& rect_for_right_prompt,
    LayoutImmediateContext& context) {
  SDL_assert(current_menu_ == MenuItem::kOptions);
  if (IsItemBeingPressed(rect_for_left_prompt, context)) {
    HandleSettingsPrompts(settings_index, true);
  } else if (IsItemBeingPressed(rect_for_right_prompt, context)) {
    HandleSettingsPrompts(settings_index, false);
  }
}
