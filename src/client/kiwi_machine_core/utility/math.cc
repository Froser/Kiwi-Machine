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

#include "utility/math.h"

#include <cmath>

bool operator==(const SDL_Rect& lhs, const SDL_Rect& rhs) {
  return SDL_RectEquals(&lhs, &rhs);
}

SDL_Rect Lerp(const SDL_Rect& start, const SDL_Rect& end, float percentage) {
  return SDL_Rect{static_cast<int>(start.x + (end.x - start.x) * percentage),
                  static_cast<int>(start.y + (end.y - start.y) * percentage),
                  static_cast<int>(start.w + (end.w - start.w) * percentage),
                  static_cast<int>(start.h + (end.h - start.h) * percentage)};
}

SDL_Rect Triangle::BoundingBox() {
  SDL_Point top_left;
  SDL_Point bottom_right;
  top_left.x = std::min(std::min(point[0].x, point[1].x), point[2].x);
  top_left.y = std::min(std::min(point[0].y, point[1].y), point[2].y);
  bottom_right.x = std::max(std::max(point[0].x, point[1].x), point[2].x);
  bottom_right.y = std::max(std::max(point[0].y, point[1].y), point[2].y);
  return SDL_Rect{top_left.x, top_left.y, bottom_right.x - top_left.x,
                  bottom_right.y - top_left.y};
}