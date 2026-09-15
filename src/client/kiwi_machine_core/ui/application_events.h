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

#ifndef UI_APPLICATION_EVENTS_H_
#define UI_APPLICATION_EVENTS_H_

#include <cstdint>

namespace application_events {

bool RequiresBatterySaveFlush(uint32_t event_type);

}  // namespace application_events

#endif  // UI_APPLICATION_EVENTS_H_
