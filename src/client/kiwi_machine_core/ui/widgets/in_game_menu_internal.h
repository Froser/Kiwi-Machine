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

#ifndef UI_WIDGETS_IN_GAME_MENU_INTERNAL_H_
#define UI_WIDGETS_IN_GAME_MENU_INTERNAL_H_

#include <SDL.h>
#include <imgui.h>

#include <string>

#include "utility/fonts.h"

namespace in_game_menu_internal {

extern const ImU32 kTextColor;
extern const ImU32 kMutedTextColor;
extern const ImU32 kStrongBorderColor;
extern const ImU32 kBrandColor;
extern const ImU32 kBrandOnColor;
extern const float kCornerRadius;

SDL_Rect MakeRect(float x, float y, float width, float height);
SDL_Rect InsetRect(const SDL_Rect& rect, float amount);
float RectRight(const SDL_Rect& rect);
float RectBottom(const SDL_Rect& rect);
ImVec2 RectMin(const SDL_Rect& rect);
ImVec2 RectMax(const SDL_Rect& rect);
PreferredFontSize GetSecondaryFontSize(PreferredFontSize size);
void DrawTextInRect(const std::string& text,
                    const SDL_Rect& rect,
                    PreferredFontSize font_size,
                    ImU32 color,
                    float horizontal_padding,
                    bool center,
                    bool wrap,
                    FontType default_font = FontType::kDefault,
                    float font_scale = 1.f);
void DrawChevron(const SDL_Rect& rect, bool points_left, ImU32 color);
void DrawSpinner(const SDL_Rect& rect);

}  // namespace in_game_menu_internal

#endif  // UI_WIDGETS_IN_GAME_MENU_INTERNAL_H_
