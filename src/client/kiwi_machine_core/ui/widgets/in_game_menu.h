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

#ifndef UI_WIDGETS_IN_GAME_MENU_H_
#define UI_WIDGETS_IN_GAME_MENU_H_

#include <kiwi_nes.h>

#if KIWI_MOBILE
#include <unordered_map>
#include <vector>
#endif

#include "build/kiwi_defines.h"
#include "models/nes_runtime.h"
#include "ui/widgets/loading_widget.h"
#include "ui/widgets/widget.h"

class MainWindow;
class InGameMenu : public Widget {
 public:
  enum {
    kMaxScaling = 4,
  };

  enum class MenuItem {
    kContinue,
    kLoadAutoSave,
    kLoadState,
    kSaveState,
    kOptions,
    kResetGame,
    kToGameSelection,

    kMax,
  };

  enum class SettingsItem {
    kVolume,
    kWindowSize,
    kJoyP1,
    kJoyP2,
    kLanguage,

    kMax,
  };

  using MenuItemCallback = kiwi::base::RepeatingCallback<void(MenuItem, int)>;
  using SettingsItemCallback =
      kiwi::base::RepeatingCallback<void(SettingsItem, bool)>;

  explicit InGameMenu(MainWindow* main_window,
                      NESRuntimeID runtime_id,
                      MenuItemCallback menu_callback,
                      SettingsItemCallback settings_callback);
  ~InGameMenu() override;
  void Close();
  void Show();
  void RequestCurrentThumbnail();
  void RequestCurrentSaveStatesCount();
  void OnGotState(const NESRuntime::Data::StateResult& result);
  void HideMenu(int index);

 private:
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  void HandleMenuItemForCurrentSelection();
  void HandleSettingsItemForCurrentSelection(bool go_left);
  void MoveSelection(bool up);
  void SetFirstSelection();

 protected:
  // Widget:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) override;

#if KIWI_MOBILE
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;
  bool HandleFingerDownOrMove(SDL_TouchFingerEvent* event);

 private:
  void AddRectForSettingsItem(int settings_index,
                              const SDL_Rect& rect_for_left,
                              const SDL_Rect& rect_for_right);
  void CleanUpSettingsItemRects();
#endif

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  bool first_paint_ = true;
  MenuItem current_selection_ = MenuItem::kContinue;
  SettingsItem current_setting_ = SettingsItem::kVolume;
  bool settings_entered_ = false;
  MenuItemCallback menu_callback_;
  SettingsItemCallback settings_callback_;
  int which_state_ = 0;
  int state_timestamp_ = 0;
  int which_autosave_state_slot_ = 0;
  std::set<int> hide_menus_;

  // Snapshot
  std::unique_ptr<LoadingWidget> loading_widget_;
  bool is_loading_snapshot_ = false;
  SDL_Texture* snapshot_ = nullptr;
  bool currently_has_snapshot_ = false;
  int current_auto_states_count_ = 0;

#if KIWI_MOBILE
  // Menu and settings positions to handle finger touch events
  std::unordered_map<MenuItem, SDL_Rect> menu_positions_;
  std::unordered_map<SettingsItem, SDL_Rect> settings_positions_;

  // std::pair<SDL_Rect, bool> saves button's position and whether it is
  // left or right button.
  std::unordered_map<SettingsItem, std::vector<std::pair<SDL_Rect, bool>>>
      settings_prompt_positions_;
#endif
};

#endif  // UI_WIDGETS_IN_GAME_MENU_H_