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

#ifndef DEBUG_DEBUG_ROMS
#define DEBUG_DEBUG_ROMS

#include "ui/widgets/menu_bar.h"

using DebugRomsLoadCallback =
    kiwi::base::RepeatingCallback<void(kiwi::base::FilePath)>;

bool HasDebugRoms();

MenuBar::MenuItem CreateDebugRomsMenu(DebugRomsLoadCallback open_callback);

#endif  // DEBUG_DEBUG_ROMS