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

namespace flex_items_widget {

int GetItemHeightHint();
int GetItemHighlightedSize();

}  // namespace flex_items_widget

namespace flex_item_widget {

int GetBadgeSize();

}  // namespace flex_item_widget

namespace in_game_menu {

PreferredFontSize GetPreferredFontSize(float window_scale);
int GetSnapshotThumbnailWidth(bool is_landscape, float window_scale);
int GetSnapshotThumbnailHeight(bool is_landscape, float window_scale);
int GetSnapshotPromptHeight(float window_scale);
int GetOptionsSpacing();

}  // namespace in_game_menu

namespace main_window {

int GetJoystickSize(float window_scale);
int GetJoystickMarginX(float window_scale,
                       bool is_landscape,
                       const SDL_Rect& safe_area_insets);
int GetJoystickMarginY(float window_scale,
                       bool is_landscape,
                       const SDL_Rect& safe_area_insets);
int GetJoystickButtonMarginX(float window_scale,
                             bool is_landscape,
                             const SDL_Rect& safe_area_insets);
int GetJoystickButtonMarginY(float window_scale,
                             bool is_landscape,
                             const SDL_Rect& safe_area_insets);
int GetJoystickSelectStartButtonMarginBottom(float window_scale,
                                             bool is_landscape,
                                             const SDL_Rect& safe_area_insets);
int GetJoystickPauseButtonMarginX(float window_scale,
                                  const SDL_Rect& safe_area_insets);
int GetJoystickPauseButtonMarginY(float window_scale,
                                  const SDL_Rect& safe_area_insets);

}  // namespace main_window

namespace side_menu {

int GetItemHeight();
int GetMarginBottom();
PreferredFontSize GetPreferredFontSize();

}  // namespace side_menu

namespace about_widget {

int GetMarginX(float window_scale);

}

}  // namespace styles

#endif  // UI_WINDOW_STYLES_H_