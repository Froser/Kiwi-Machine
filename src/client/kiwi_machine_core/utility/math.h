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

#ifndef UTILITY_MATH_H_
#define UTILITY_MATH_H_

#include <SDL.h>
#include <imgui.h>
#include <kiwi_nes.h>
#include <algorithm>

SDL_Rect Lerp(const SDL_Rect& start, const SDL_Rect& end, float percentage);

inline float Lerp(float start, float end, float percentage) {
  percentage = std::clamp(percentage, 0.f, 1.f);
  return start + (end - start) * percentage;
}

inline bool Contains(const SDL_Rect& rect, int x, int y) {
  SDL_Point temp{x, y};
  return SDL_PointInRect(&temp, &rect);
}

inline bool Intersect(const SDL_Rect& lhs, const SDL_Rect& rhs) {
  return SDL_HasIntersection(&lhs, &rhs);
}

struct Triangle {
  ImVec2 point[3];
  SDL_Rect BoundingBox();
};

inline SDL_Rect Center(const SDL_Rect& parent, const SDL_Rect& rect) {
  return SDL_Rect{parent.x + (parent.w - rect.w) / 2,
                  parent.y + (parent.h - rect.h) / 2, rect.w, rect.h};
}

#endif  // UTILITY_MATH_H_