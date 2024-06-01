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

bool InGameMenu::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  return HandleMouseOrFingerEvents(MouseOrFingerEventType::kFingerDown,
                                   event->x * bounds().w,
                                   event->y * bounds().h);
}

bool InGameMenu::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  return HandleMouseOrFingerEvents(MouseOrFingerEventType::kFingerMove,
                                   event->x * bounds().w,
                                   event->y * bounds().h);
}
