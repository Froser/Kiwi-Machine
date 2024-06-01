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

#ifndef UI_WINDOW_STYLES_H_
#define UI_WINDOW_STYLES_H_

#include <SDL_rect.h>

#include "utility/fonts.h"

namespace styles {

namespace kiwi_item_widget {

int GetSpacingBetweenTitleAndCover();
PreferredFontSize GetGameTitlePreferredFontSize();
int GetItemMetrics(float window_scale, int metrics);

}  // namespace kiwi_item_widget

namespace kiwi_bg_widget {

int GetKiwiPositionX(const SDL_Rect& safe_area_insets);
int GetKiwiPositionY(const SDL_Rect& safe_area_insets);
float GetKiwiScale(float window_scale);

}  // namespace kiwi_bg_widget

namespace in_game_menu {

int GetSnapshotThumbnailWidth(bool is_landscape, float window_scale);
int GetSnapshotThumbnailHeight(bool is_landscape, float window_scale);
int GetSnapshotPromptHeight(float window_scale);
int GetOptionsSpacing();
FontType GetJoystickFontType(const char* str_hint,
                             PreferredFontSize fallback_font_size);
FontType GetSlotNameFontType(bool is_landscape, const char* str_hint);

}  // namespace in_game_menu

namespace main_window {

int GetJoystickSize(float window_scale);
int GetJoystickPaddingX(float window_scale,
                        bool is_landscape,
                        const SDL_Rect& safe_area_insets);
int GetJoystickPaddingY(float window_scale,
                        bool is_landscape,
                        const SDL_Rect& safe_area_insets);
int GetJoystickButtonPaddingX(float window_scale,
                              bool is_landscape,
                              const SDL_Rect& safe_area_insets);
int GetJoystickButtonPaddingY(float window_scale,
                              bool is_landscape,
                              const SDL_Rect& safe_area_insets);
int GetJoystickSelectStartButtonPaddingBottom(float window_scale,
                                              bool is_landscape,
                                              const SDL_Rect& safe_area_insets);
int GetJoystickPauseButtonPaddingX(float window_scale,
                                   const SDL_Rect& safe_area_insets);
int GetJoystickPauseButtonPaddingY(float window_scale,
                                   const SDL_Rect& safe_area_insets);

}  // namespace main_window

}  // namespace styles

#endif  // UI_WINDOW_STYLES_H_