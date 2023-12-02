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

#include <SDL.h>

#include "ui/window_base.h"
#include "utility/math.h"

bool InGameMenu::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  return HandleFingerDownOrMove(event);
}

bool InGameMenu::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  return HandleFingerDownOrMove(event);
}

bool InGameMenu::HandleFingerDownOrMove(SDL_TouchFingerEvent* event) {
  // Moving finger only affects selection. It won't trigger the menu item.
  bool suppress_trigger = event->type == SDL_FINGERMOTION;
  SDL_Rect bounds = window()->GetClientBounds();
  int x = event->x * bounds.w;
  int y = event->y * bounds.h;

  // Find if touched any menu item.
  for (const auto& menu : menu_positions_) {
    if (Contains(menu.second, x, y)) {
      if (settings_entered_)
        settings_entered_ = false;
      if (current_selection_ != menu.first)
        current_selection_ = menu.first;
      else if (!suppress_trigger)
        HandleMenuItemForCurrentSelection();
      return true;
    }
  }

  // Find if touched any settings item. Ignoring finger moving.
  if (event->type == SDL_FINGERDOWN) {
    // Find settings item first.
    for (const auto& setting : settings_positions_) {
      // If the setting hasn't been entered, enter the setting first.
      // If current setting item has already been selected, propagates the event
      // to its prompt.
      if (Contains(setting.second, x, y) &&
          (current_setting_ != setting.first || !settings_entered_)) {
        settings_entered_ = true;
        current_setting_ = setting.first;
        return true;
      }
    }

    // Find prompt.
    for (const auto& prompt_position : settings_prompt_positions_) {
      for (const auto& prompt : prompt_position.second) {
        // Prompt.first represents the button position
        // Prompt.second represents whether it is a left prompt or a right
        // prompt.
        if (Contains(prompt.first, x, y)) {
          current_setting_ = prompt_position.first;
          HandleSettingsItemForCurrentSelection(prompt.second);
          return true;
        }
      }
    }
  }

  return false;
}

void InGameMenu::AddRectForSettingsItem(int settings_index,
                                        const SDL_Rect& rect_for_left,
                                        const SDL_Rect& rect_for_right) {
  settings_prompt_positions_[static_cast<SettingsItem>(settings_index)]
      .push_back(std::make_pair(rect_for_left, true));
  settings_prompt_positions_[static_cast<SettingsItem>(settings_index)]
      .push_back(std::make_pair(rect_for_right, false));
}

void InGameMenu::CleanUpSettingsItemRects() {
  settings_positions_.clear();
  settings_prompt_positions_.clear();
}
