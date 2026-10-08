// Copyright (C) 2026 Yisi Yu
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

#ifndef UTILITY_SWITCH_SYSTEM_KEYBOARD_H_
#define UTILITY_SWITCH_SYSTEM_KEYBOARD_H_

#include <stddef.h>

#include <string>

namespace kiwi::switch_platform {

// Shows Nintendo Switch's system software keyboard. Returns true only when the
// user confirms the input; cancellation and launch failures leave |output|
// unchanged.
bool ShowSystemKeyboard(const std::string& title,
                        const std::string& confirm_text,
                        const std::string& initial_text,
                        size_t max_characters,
                        std::string* output);

}  // namespace kiwi::switch_platform

#endif  // UTILITY_SWITCH_SYSTEM_KEYBOARD_H_
