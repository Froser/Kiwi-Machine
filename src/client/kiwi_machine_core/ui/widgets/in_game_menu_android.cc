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
  return HandleFingerDownOrMove(event, false);
}

bool InGameMenu::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  // Moving finger only affects selection. It won't trigger the menu item.
  return HandleFingerDownOrMove(event, true);
}

bool InGameMenu::HandleFingerDownOrMove(SDL_TouchFingerEvent* event,
                                        bool suppress_trigger) {
  SDL_Rect bounds = window()->GetClientBounds();
  int x = event->x * bounds.w;
  int y = event->y * bounds.h;
  for (const auto& menu : menu_positions_) {
    if (Contains(menu.second, x, y)) {
      if (settings_entered_)
        settings_entered_ = false;
      MenuItem current_item = static_cast<MenuItem>(menu.first);
      if (current_selection_ != current_item)
        current_selection_ = current_item;
      else if (!suppress_trigger)
        HandleMenuItemForCurrentSelection();
      return true;
    }
  }
  return false;
}