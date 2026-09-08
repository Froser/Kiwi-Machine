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

#ifndef UI_WINDOW_BASE_MACOS_H_
#define UI_WINDOW_BASE_MACOS_H_

#include <SDL.h>

#include "ui/widgets/widget.h"

void InitializeMacMouseWheelPhaseMonitor();
void UninitializeMacMouseWheelPhaseMonitor();
bool DecodeMacMouseWheelPhaseEvent(const SDL_Event* event,
                                   MouseWheelPhaseEvent* phase_event);

#endif  // UI_WINDOW_BASE_MACOS_H_
