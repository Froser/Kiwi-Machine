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

#include "ui/widgets/hd_edition_badge.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace {

constexpr float kDesignWidth = 52.f;
constexpr float kDesignHeight = 24.f;
constexpr float kCoverBadgeWidthRatio = .42f;
constexpr float kCoverBadgeHeightRatio = .32f;
constexpr float kAnimationDurationMs = 3200.f;
constexpr float kSweepEnd = .26f;

constexpr ImU32 kFrameShadowColor = IM_COL32(0, 13, 18, 230);
constexpr ImU32 kFrameColor = IM_COL32(19, 218, 255, 235);
constexpr ImU32 kBadgeShadowColor = IM_COL32(0, 0, 0, 150);
constexpr ImU32 kBadgeFaceColor = IM_COL32(5, 19, 29, 248);
constexpr ImU32 kBadgeAccentColor = IM_COL32(19, 218, 255, 255);
constexpr ImU32 kBadgeAccentDarkColor = IM_COL32(0, 113, 145, 255);
constexpr ImU32 kPrimaryTextColor = IM_COL32(255, 255, 255, 255);

ImVec2 ScalePoint(const ImVec2& origin, float scale, float x, float y) {
  return ImVec2(origin.x + x * scale, origin.y + y * scale);
}

template <std::size_t N>
std::array<ImVec2, N> ScalePoints(const ImVec2& origin,
                                  float scale,
                                  const std::array<ImVec2, N>& design_points) {
  std::array<ImVec2, N> points;
  for (std::size_t i = 0; i < N; ++i) {
    points[i] =
        ScalePoint(origin, scale, design_points[i].x, design_points[i].y);
  }
  return points;
}

template <std::size_t N>
void AddConvexPolygon(ImDrawList* draw_list,
                      const ImVec2& origin,
                      float scale,
                      const std::array<ImVec2, N>& design_points,
                      ImU32 color) {
  const std::array<ImVec2, N> points =
      ScalePoints(origin, scale, design_points);
  draw_list->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()),
                                 color);
}

void DrawHDLabel(ImDrawList* draw_list,
                 const ImVec2& center,
                 float scale,
                 ImU32 color) {
  constexpr float kGlyphHeight = 14.f;
  constexpr float kGlyphWidth = 6.5f;
  constexpr float kGlyphGap = 3.f;
  constexpr float kLabelWidth = kGlyphWidth * 2.f + kGlyphGap;

  const float left = center.x - kLabelWidth * scale * .5f;
  const float top = center.y - kGlyphHeight * scale * .5f;
  const float bottom = center.y + kGlyphHeight * scale * .5f;
  const float glyph_width = kGlyphWidth * scale;
  const float thickness = std::max(1.f, 1.8f * scale);

  draw_list->AddLine(ImVec2(left, top), ImVec2(left, bottom), color, thickness);
  draw_list->AddLine(ImVec2(left + glyph_width, top),
                     ImVec2(left + glyph_width, bottom), color, thickness);
  draw_list->AddLine(ImVec2(left, center.y),
                     ImVec2(left + glyph_width, center.y), color, thickness);

  const float d_left = left + (kGlyphWidth + kGlyphGap) * scale;
  draw_list->AddLine(ImVec2(d_left, top), ImVec2(d_left, bottom), color,
                     thickness);
  draw_list->AddBezierCubic(ImVec2(d_left, top),
                            ImVec2(d_left + glyph_width * 1.25f, top),
                            ImVec2(d_left + glyph_width * 1.25f, bottom),
                            ImVec2(d_left, bottom), color, thickness);
}

float SmoothStep(float progress) {
  progress = std::clamp(progress, 0.f, 1.f);
  return progress * progress * (3.f - 2.f * progress);
}

ImU32 WhiteWithAlpha(float alpha) {
  return IM_COL32(255, 255, 255,
                  static_cast<int>(std::clamp(alpha, 0.f, 1.f) * 255.f));
}

ImU32 ApplyOpacity(ImU32 color, float opacity) {
  const ImU32 original_alpha = (color & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
  const ImU32 alpha =
      static_cast<ImU32>(original_alpha * std::clamp(opacity, 0.f, 1.f));
  return (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
}

}  // namespace

void HDEditionBadge::Paint(ImDrawList* draw_list,
                           const SDL_Rect& cover_bounds,
                           bool animate,
                           float opacity,
                           bool draw_cover_frame,
                           Presentation presentation) {
  if (!draw_list || cover_bounds.w <= 0 || cover_bounds.h <= 0)
    return;

  if (animate && !was_animated_)
    animation_timer_.Reset();
  was_animated_ = animate;

  const float cover_width = static_cast<float>(cover_bounds.w);
  const float cover_height = static_cast<float>(cover_bounds.h);
  const float shortest_edge =
      static_cast<float>(std::min(cover_bounds.w, cover_bounds.h));
  const bool is_cover_overlay = presentation == Presentation::kCoverOverlay;
  const float badge_width =
      is_cover_overlay
          ? std::min(cover_width * kCoverBadgeWidthRatio,
                     cover_height * kCoverBadgeHeightRatio)
          : std::min(cover_width, cover_height * kDesignWidth / kDesignHeight);
  const float scale = badge_width / kDesignWidth;
  const float badge_height = kDesignHeight * scale;
  const float margin = std::max(2.f, 3.f * scale);
  const ImVec2 origin(
      is_cover_overlay ? cover_bounds.x + margin
                       : cover_bounds.x + (cover_width - badge_width) * .5f,
      is_cover_overlay ? cover_bounds.y + margin
                       : cover_bounds.y + (cover_height - badge_height) * .5f);
  const ImVec2 cover_min(static_cast<float>(cover_bounds.x),
                         static_cast<float>(cover_bounds.y));
  const ImVec2 cover_max(static_cast<float>(cover_bounds.x + cover_bounds.w),
                         static_cast<float>(cover_bounds.y + cover_bounds.h));
  const float frame_width = std::clamp(shortest_edge * .015f, 1.5f, 5.f);
  const ImU32 frame_shadow_color = ApplyOpacity(kFrameShadowColor, opacity);
  const ImU32 frame_color = ApplyOpacity(kFrameColor, opacity);
  const ImU32 badge_shadow_color = ApplyOpacity(kBadgeShadowColor, opacity);
  const ImU32 badge_face_color = ApplyOpacity(kBadgeFaceColor, opacity);
  const ImU32 badge_accent_color = ApplyOpacity(kBadgeAccentColor, opacity);
  const ImU32 badge_accent_dark_color =
      ApplyOpacity(kBadgeAccentDarkColor, opacity);
  const ImU32 primary_text_color = ApplyOpacity(kPrimaryTextColor, opacity);

  draw_list->PushClipRect(cover_min, cover_max, true);

  if (draw_cover_frame) {
    draw_list->AddRect(cover_min, cover_max, frame_shadow_color, 0.f,
                       ImDrawFlags_None, frame_width * 2.6f);
    draw_list->AddRect(cover_min, cover_max, frame_color, 0.f, ImDrawFlags_None,
                       frame_width);
  }

  const float phase =
      std::fmod(static_cast<float>(animation_timer_.ElapsedInMilliseconds()),
                kAnimationDurationMs) /
      kAnimationDurationMs;

  constexpr std::array<ImVec2, 5> kBadgeShape = {
      ImVec2(0.f, 0.f), ImVec2(46.f, 0.f), ImVec2(52.f, 12.f),
      ImVec2(46.f, kDesignHeight), ImVec2(0.f, kDesignHeight)};
  const ImVec2 shadow_origin = ScalePoint(origin, scale, 1.5f, 2.f);
  AddConvexPolygon(draw_list, shadow_origin, scale, kBadgeShape,
                   badge_shadow_color);
  AddConvexPolygon(draw_list, origin, scale, kBadgeShape, badge_face_color);
  AddConvexPolygon(draw_list, origin, scale,
                   std::array<ImVec2, 4>{ImVec2(0.f, 0.f), ImVec2(3.f, 0.f),
                                         ImVec2(3.f, kDesignHeight),
                                         ImVec2(0.f, kDesignHeight)},
                   badge_accent_color);
  AddConvexPolygon(draw_list, origin, scale,
                   std::array<ImVec2, 3>{ImVec2(46.f, 0.f), ImVec2(52.f, 12.f),
                                         ImVec2(46.f, kDesignHeight)},
                   badge_accent_dark_color);

  const std::array<ImVec2, 5> badge_outline =
      ScalePoints(origin, scale, kBadgeShape);
  draw_list->AddPolyline(
      badge_outline.data(), static_cast<int>(badge_outline.size()),
      badge_accent_color, ImDrawFlags_Closed, std::max(1.f, scale));
  draw_list->AddLine(ScalePoint(origin, scale, 7.f, 21.f),
                     ScalePoint(origin, scale, 44.f, 21.f), badge_accent_color,
                     std::max(1.f, scale));

  if (animate && phase <= kSweepEnd) {
    const float sweep_progress = phase / kSweepEnd;
    const float eased_progress = SmoothStep(sweep_progress);
    const float fade =
        std::min({sweep_progress / .16f, (1.f - sweep_progress) / .16f, 1.f});
    const float center_x = -6.f + eased_progress * 53.f;
    const std::array<ImVec2, 4> shine_points = ScalePoints(
        origin, scale,
        std::array<ImVec2, 4>{
            ImVec2(center_x - 3.f, 2.f), ImVec2(center_x + 1.f, 2.f),
            ImVec2(center_x - 5.f, 21.f), ImVec2(center_x - 9.f, 21.f)});
    draw_list->AddConvexPolyFilled(shine_points.data(),
                                   static_cast<int>(shine_points.size()),
                                   WhiteWithAlpha(fade * .42f * opacity));
  }

  DrawHDLabel(draw_list, ScalePoint(origin, scale, 23.f, 11.f), scale,
              primary_text_color);

  draw_list->PopClipRect();
}
