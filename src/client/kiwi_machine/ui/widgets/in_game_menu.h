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

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"

class MainWindow;
class InGameMenu : public Widget {
 public:
  enum class MenuItem {
    kContinue,
    kLoadState,
    kSaveState,
    kOptions,
    kToGameSelection,

    kMax,
  };

  enum class SettingsItem {
    kVolume,
    kWindowSize,

    kMax,
  };

  using MenuItemCallback = kiwi::base::RepeatingCallback<void(MenuItem)>;
  using SettingsItemCallback =
      kiwi::base::RepeatingCallback<void(SettingsItem, bool)>;

  explicit InGameMenu(MainWindow* main_window,
                      NESRuntimeID runtime_id,
                      MenuItemCallback menu_callback,
                      SettingsItemCallback settings_callback);
  ~InGameMenu() override;
  void Close();
  void Show();
  void NotifyThumbnailChanged();
  void HideMenu(int index);

 private:
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  void MoveSelection(bool up);

 protected:
  // Widget:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) override;

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  bool first_paint_ = true;
  MenuItem current_selection_ = MenuItem::kContinue;
  SettingsItem current_setting_ = SettingsItem::kVolume;
  bool settings_entered_ = false;
  MenuItemCallback menu_callback_;
  SettingsItemCallback settings_callback_;
  std::set<int> hide_menus_;

  // Snapshot
  SDL_Texture* snapshot_ = nullptr;
};

#endif  // UI_WIDGETS_IN_GAME_MENU_H_