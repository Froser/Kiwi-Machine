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

#ifndef UTILITY_KEY_MAPPING_UTIL_H_
#define UTILITY_KEY_MAPPING_UTIL_H_

#include <SDL.h>
#include <kiwi_nes.h>

#include "models/nes_runtime.h"

bool IsJoystickButtonMatch(NESRuntime::Data* runtime_data,
                           kiwi::nes::ControllerButton button,
                           SDL_Keysym key);

bool IsJoystickAxisMotionMatch(kiwi::nes::ControllerButton button);

bool IsKeyboardOrControllerAxisMotionMatch(NESRuntime::Data* runtime_data,
                                           kiwi::nes::ControllerButton button,
                                           SDL_KeyboardEvent* k);

void SetControllerMapping(NESRuntime::Data* runtime_data,
                          int player,
                          SDL_GameController* controller,
                          bool ab_reverse);

std::vector<SDL_GameController*> GetControllerList();

#endif  // UTILITY_KEY_MAPPING_UTIL_H_