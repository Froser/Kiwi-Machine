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

#include "ui/styles.h"

#include "build/kiwi_defines.h"
#include "ui/widgets/canvas.h"
#include "utility/fonts.h"

namespace styles {

namespace flex_items_widget {

int GetItemHeightHint() {
#if KIWI_ANDROID
  return 480;
#else
  return 160;
#endif
}

int GetItemHighlightedSize() {
#if KIWI_ANDROID
  return 50;
#else
  return 20;
#endif
}

PreferredFontSize GetDetailFontSize() {
#if KIWI_ANDROID
  return PreferredFontSize::k3x;
#else
  return PreferredFontSize::k1x;
#endif
}

PreferredFontSize GetFilterFontSize() {
  return GetDetailFontSize();
}

}  // namespace flex_items_widget

namespace flex_item_widget {

int GetBadgeSize() {
#if KIWI_ANDROID
  return 96;
#else
  return 32;
#endif
}

}  // namespace flex_item_widget

namespace in_game_menu {

PreferredFontSize GetPreferredFontSize(float window_scale) {
#if KIWI_IOS
  return PreferredFontSize::k1x;
#elif KIWI_ANDROID
  return PreferredFontSize::k3x;
#else
  return window_scale > 2.f ? PreferredFontSize::k2x : PreferredFontSize::k1x;
#endif
}

int GetSnapshotThumbnailWidth(bool is_landscape, float window_scale) {
#if KIWI_IOS
  return Canvas::kNESFrameDefaultWidth / (is_landscape ? 2 : 3) * window_scale;
#else
  return Canvas::kNESFrameDefaultWidth / 3 * window_scale;
#endif
}

int GetSnapshotThumbnailHeight(bool is_landscape, float window_scale) {
#if KIWI_IOS
  return Canvas::kNESFrameDefaultHeight / (is_landscape ? 2 : 3) * window_scale;
#else
  return Canvas::kNESFrameDefaultHeight / 3 * window_scale;
#endif
}

int GetSnapshotPromptHeight(float window_scale) {
#if KIWI_MOBILE
  return 14 * window_scale;
#else
  return 7 * window_scale;
#endif
}

int GetOptionsSpacing() {
#if KIWI_IOS
  return 7;
#else
  return 20;
#endif
}

}  // namespace in_game_menu

namespace main_window {

int GetJoystickSize(float window_scale) {
  return 135 * window_scale;
}

int GetJoystickMarginX(float window_scale,
                       bool is_landscape,
                       const SDL_Rect& safe_area_insets) {
  int result = is_landscape ? 26 * window_scale : 10 * window_scale;
  return result + safe_area_insets.x;
}

int GetJoystickMarginY(float window_scale,
                       bool is_landscape,
                       const SDL_Rect& safe_area_insets) {
#if KIWI_ANDROID
  int result = is_landscape ? 20 * window_scale : 40 * window_scale;
#else
  int result = is_landscape ? 26 * window_scale : 10 * window_scale;
#endif
  return result + safe_area_insets.h;
}

int GetJoystickButtonMarginX(float window_scale,
                             bool is_landscape,
                             const SDL_Rect& safe_area_insets) {
  int result = is_landscape ? 40 * window_scale : 30 * window_scale;
  return result + safe_area_insets.x;
}

int GetJoystickButtonMarginY(float window_scale,
                             bool is_landscape,
                             const SDL_Rect& safe_area_insets) {
  int result = is_landscape ? 40 * window_scale : 60 * window_scale;
  return result + safe_area_insets.h;
}

int GetJoystickSelectStartButtonMarginBottom(float window_scale,
                                             bool is_landscape,
                                             const SDL_Rect& safe_area_insets) {
  int result = is_landscape ? 30 * window_scale : 10 * window_scale;
  return result + safe_area_insets.h;
}

int GetJoystickPauseButtonMarginX(float window_scale,
                                  const SDL_Rect& safe_area_insets) {
  return 33 * window_scale + safe_area_insets.x;
}

int GetJoystickPauseButtonMarginY(float window_scale,
                                  const SDL_Rect& safe_area_insets) {
  return 33 * window_scale + safe_area_insets.y;
}

}  // namespace main_window

namespace side_menu {

int GetItemHeight() {
#if KIWI_ANDROID
  return 80;
#else
  return 20;
#endif
}

int GetButtonHeight() {
  return GetItemHeight();
}

int GetMarginBottom() {
#if KIWI_MOBILE
  // Many mobile screen has a rounded corner, we set margin as a larger value
  // here.
  return 80;
#else
  return 15;
#endif
}

PreferredFontSize GetPreferredFontSize() {
#if KIWI_ANDROID
  return PreferredFontSize::k2x;
#else
  return PreferredFontSize::k1x;
#endif
}

}  // namespace side_menu

namespace about_widget {

int GetMarginX(float window_scale) {
#if KIWI_MOBILE
  return 120;
#else
  if (window_scale > 2.f)
    return 40;
  return 20;
#endif
}

PreferredFontSize PreferredTitleFontSize(float window_scale) {
#if KIWI_MOBILE
  return PreferredFontSize::k3x;
#else
  return window_scale > 2.f ? PreferredFontSize::k2x : PreferredFontSize::k1x;
#endif
}

PreferredFontSize PreferredContentFontSize() {
#if KIWI_MOBILE
  return PreferredFontSize::k2x;
#else
  return PreferredFontSize::k1x;
#endif
}

}  // namespace about_widget

namespace toast {

SDL_Point GetTopLeft() {
#if KIWI_MOBILE
  static SDL_Point kPoint = {30, 60};
#else
  static SDL_Point kPoint = {10, 20};
#endif
  return kPoint;
}

PreferredFontSize GetFontSize() {
#if KIWI_MOBILE
  return PreferredFontSize::k3x;
#else
  return PreferredFontSize::k2x;
#endif
}

}  // namespace toast

namespace filter_widget {

int GetTitleTop(const SDL_Rect& global_bounds, const ImVec2& combined_rect) {
#if KIWI_MOBILE
  return 20;
#else
  return (global_bounds.h - combined_rect.y) / 2;
#endif
}

}  // namespace filter_widget

}  // namespace styles
