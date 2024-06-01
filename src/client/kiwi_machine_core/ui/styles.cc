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

namespace kiwi_item_widget {

int GetSpacingBetweenTitleAndCover() {
#if KIWI_ANDROID
  return 48;
#else
  return 16;
#endif
}

PreferredFontSize GetGameTitlePreferredFontSize() {
#if KIWI_ANDROID
  return PreferredFontSize::k2x;
#else
  return PreferredFontSize::k1x;
#endif
}

int GetItemMetrics(float window_scale, int metrics) {
#if !KIWI_IOS
  return static_cast<int>(metrics) * window_scale;
#else
  return static_cast<int>(metrics) * 2;
#endif
}

}  // namespace kiwi_item_widget

namespace kiwi_bg_widget {

int GetKiwiPositionX(const SDL_Rect& safe_area_insets) {
  return 30 + safe_area_insets.x;
}

int GetKiwiPositionY(const SDL_Rect& safe_area_insets) {
  return 30 + safe_area_insets.y;
}

float GetKiwiScale(float window_scale) {
#if !KIWI_IOS
  return 0.08f * window_scale;
#else
  return 0.16f;
#endif
}

}  // namespace kiwi_bg_widget

namespace in_game_menu {

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
#if KIWI_IOS
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

FontType GetJoystickFontType(bool is_fullscreen,
                             float window_scale,
                             const char* str_hint) {
#if KIWI_ANDROID
  return FontType::kDefault2x;
#elif KIWI_IOS
  return FontType::kDefault;
#else
  PreferredFontSize font_size(
      window_scale > 3.f ? PreferredFontSize::k3x
                         : (window_scale > 2.f ? PreferredFontSize::k2x
                                               : PreferredFontSize::k1x));
  return GetPreferredFontType(font_size, str_hint);
#endif
}

FontType GetSlotNameFontType(bool is_landscape, const char* str_hint) {
#if KIWI_IOS
  return is_landscape ? FontType::kDefault : FontType::kSystemDefault;
#else
  return GetPreferredFontType(PreferredFontSize::k1x, str_hint);
#endif
}

}  // namespace in_game_menu

namespace main_window {

int GetJoystickSize(float window_scale) {
  return 135 * window_scale;
}

int GetJoystickPaddingX(float window_scale,
                        bool is_landscape,
                        const SDL_Rect& safe_area_insets) {
#if KIWI_ANDROID
  int result = is_landscape ? 18 * window_scale : 10 * window_scale;
#else
  int result = is_landscape ? 26 * window_scale : 10 * window_scale;
#endif
  return result + safe_area_insets.x;
}

int GetJoystickPaddingY(float window_scale,
                        bool is_landscape,
                        const SDL_Rect& safe_area_insets) {
#if KIWI_ANDROID
  int result = is_landscape ? 10 * window_scale : 40 * window_scale;
#else
  int result = is_landscape ? 26 * window_scale : 10 * window_scale;
#endif
  return result + safe_area_insets.h;
}

int GetJoystickButtonPaddingX(float window_scale,
                              bool is_landscape,
                              const SDL_Rect& safe_area_insets) {
  int result = is_landscape ? 40 * window_scale : 30 * window_scale;
  return result + safe_area_insets.x;
}

int GetJoystickButtonPaddingY(float window_scale,
                              bool is_landscape,
                              const SDL_Rect& safe_area_insets) {
  int result = is_landscape ? 40 * window_scale : 60 * window_scale;
  return result + safe_area_insets.h;
}

int GetJoystickSelectStartButtonPaddingBottom(
    float window_scale,
    bool is_landscape,
    const SDL_Rect& safe_area_insets) {
  int result = is_landscape ? 30 * window_scale : 10 * window_scale;
  return result + safe_area_insets.h;
}

int GetJoystickPauseButtonPaddingX(float window_scale,
                                   const SDL_Rect& safe_area_insets) {
  return 33 * window_scale + safe_area_insets.x;
}

int GetJoystickPauseButtonPaddingY(float window_scale,
                                   const SDL_Rect& safe_area_insets) {
  return 33 * window_scale + safe_area_insets.y;
}

}  // namespace main_window

}  // namespace styles
