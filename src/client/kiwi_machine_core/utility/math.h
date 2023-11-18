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
#include <kiwi_nes.h>
#include <algorithm>

SDL_Rect Lerp(const SDL_Rect& start, const SDL_Rect& end, float percentage);

inline float Lerp(float start, float end, float percentage) {
  percentage = std::clamp(percentage, 0.f, 1.f);
  return start + (end - start) * percentage;
}

inline bool Contains(const SDL_Rect& rect, int x, int y) {
  return (rect.x <= x && x <= rect.x + rect.w && rect.y <= y &&
          y <= rect.y + rect.h);
}

#endif  // UTILITY_MATH_H_