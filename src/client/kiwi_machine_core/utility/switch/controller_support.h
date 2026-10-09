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

#ifndef UTILITY_SWITCH_CONTROLLER_SUPPORT_H_
#define UTILITY_SWITCH_CONTROLLER_SUPPORT_H_

namespace kiwi::switch_platform {

// Shows Nintendo Switch's native Change Grip/Order interface. Returns true
// only when the applet completes successfully.
bool ShowControllerSupport(int min_players, int max_players);

}  // namespace kiwi::switch_platform

#endif  // UTILITY_SWITCH_CONTROLLER_SUPPORT_H_
