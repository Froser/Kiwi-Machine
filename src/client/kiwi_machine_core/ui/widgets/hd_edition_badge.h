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

#ifndef UI_WIDGETS_HD_EDITION_BADGE_H_
#define UI_WIDGETS_HD_EDITION_BADGE_H_

#include <SDL_rect.h>

#include "utility/timer.h"

struct ImDrawList;

class HDEditionBadge {
 public:
  HDEditionBadge() = default;
  ~HDEditionBadge() = default;

  // Paints the edition label and optional frame inside |cover_bounds|.
  void Paint(ImDrawList* draw_list,
             const SDL_Rect& cover_bounds,
             bool animate,
             float opacity = 1.f,
             bool draw_cover_frame = true);

 private:
  Timer animation_timer_;
  bool was_animated_ = false;
};

#endif  // UI_WIDGETS_HD_EDITION_BADGE_H_
