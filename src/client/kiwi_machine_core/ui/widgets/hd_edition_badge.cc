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
#include <cfloat>
#include <cmath>
#include <cstddef>

namespace {

constexpr float kDesignWidth = 112.f;
constexpr float kDesignHeight = 44.f;
constexpr float kBadgeCoverRatio = .78f;
constexpr float kMinimumBadgeWidth = 84.f;
constexpr float kMaximumBadgeWidth = 280.f;
constexpr float kAnimationDurationMs = 3200.f;
constexpr float kSweepEnd = .26f;

constexpr ImU32 kFrameShadowColor = IM_COL32(0, 13, 18, 230);
constexpr ImU32 kFrameColor = IM_COL32(19, 218, 255, 235);
constexpr ImU32 kBadgeShadowColor = IM_COL32(0, 0, 0, 150);
constexpr ImU32 kBadgeFaceColor = IM_COL32(5, 19, 29, 248);
constexpr ImU32 kBadgeInsetColor = IM_COL32(10, 38, 51, 255);
constexpr ImU32 kBadgeAccentColor = IM_COL32(19, 218, 255, 255);
constexpr ImU32 kBadgeAccentDarkColor = IM_COL32(0, 113, 145, 255);
constexpr ImU32 kPrimaryTextColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kSecondaryTextColor = IM_COL32(174, 244, 255, 255);

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
  constexpr float kGlyphHeight = 24.f;
  constexpr float kGlyphWidth = 10.5f;
  constexpr float kGlyphGap = 4.5f;
  constexpr float kLabelWidth = kGlyphWidth * 2.f + kGlyphGap;

  const float left = center.x - kLabelWidth * scale * .5f;
  const float top = center.y - kGlyphHeight * scale * .5f;
  const float bottom = center.y + kGlyphHeight * scale * .5f;
  const float glyph_width = kGlyphWidth * scale;
  const float thickness = std::max(1.5f, 3.2f * scale);

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

void DrawEditionLabel(ImDrawList* draw_list,
                      const ImVec2& origin,
                      float scale,
                      ImU32 color) {
  constexpr char kEditionText[] = "EDITION";
  constexpr float kTextLeft = 62.f;
  constexpr float kTextAreaWidth = 37.f;
  constexpr float kTextCenterY = 15.f;

  ImFont* font = ImGui::GetFont();
  float font_size = 10.5f * scale;
  ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, kEditionText);
  const float maximum_width = kTextAreaWidth * scale;
  if (text_size.x > maximum_width) {
    font_size *= maximum_width / text_size.x;
    text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, kEditionText);
  }

  const ImVec2 text_position(
      origin.x + (kTextLeft + kTextAreaWidth * .5f) * scale - text_size.x * .5f,
      origin.y + kTextCenterY * scale - text_size.y * .5f);
  draw_list->AddText(font, font_size, text_position, color, kEditionText);
}

void DrawPixelScale(ImDrawList* draw_list,
                    const ImVec2& origin,
                    float scale,
                    ImU32 accent_color,
                    ImU32 dark_color) {
  struct Segment {
    float left;
    float width;
    bool accented;
  };
  constexpr std::array<Segment, 4> kSegments = {{
      {62.f, 6.f, true},
      {70.f, 9.f, false},
      {81.f, 6.f, true},
      {89.f, 10.f, false},
  }};
  constexpr float kTop = 27.f;
  constexpr float kHeight = 7.f;

  for (const Segment& segment : kSegments) {
    draw_list->AddRectFilled(
        ScalePoint(origin, scale, segment.left, kTop),
        ScalePoint(origin, scale, segment.left + segment.width, kTop + kHeight),
        segment.accented ? accent_color : dark_color);
  }
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
                           bool draw_cover_frame) {
  if (!draw_list || cover_bounds.w <= 0 || cover_bounds.h <= 0)
    return;

  if (animate && !was_animated_)
    animation_timer_.Reset();
  was_animated_ = animate;

  const float cover_width = static_cast<float>(cover_bounds.w);
  const float shortest_edge =
      static_cast<float>(std::min(cover_bounds.w, cover_bounds.h));
  const float badge_width =
      std::min(cover_width, std::clamp(cover_width * kBadgeCoverRatio,
                                       kMinimumBadgeWidth, kMaximumBadgeWidth));
  const float scale = badge_width / kDesignWidth;
  const ImVec2 origin(static_cast<float>(cover_bounds.x),
                      cover_bounds.y + std::max(2.f, 5.f * scale));
  const ImVec2 cover_min(static_cast<float>(cover_bounds.x),
                         static_cast<float>(cover_bounds.y));
  const ImVec2 cover_max(static_cast<float>(cover_bounds.x + cover_bounds.w),
                         static_cast<float>(cover_bounds.y + cover_bounds.h));
  const float frame_width = std::clamp(shortest_edge * .015f, 1.5f, 5.f);
  const ImU32 frame_shadow_color = ApplyOpacity(kFrameShadowColor, opacity);
  const ImU32 frame_color = ApplyOpacity(kFrameColor, opacity);
  const ImU32 badge_shadow_color = ApplyOpacity(kBadgeShadowColor, opacity);
  const ImU32 badge_face_color = ApplyOpacity(kBadgeFaceColor, opacity);
  const ImU32 badge_inset_color = ApplyOpacity(kBadgeInsetColor, opacity);
  const ImU32 badge_accent_color = ApplyOpacity(kBadgeAccentColor, opacity);
  const ImU32 badge_accent_dark_color =
      ApplyOpacity(kBadgeAccentDarkColor, opacity);
  const ImU32 primary_text_color = ApplyOpacity(kPrimaryTextColor, opacity);
  const ImU32 secondary_text_color = ApplyOpacity(kSecondaryTextColor, opacity);

  draw_list->PushClipRect(cover_min, cover_max, true);

  if (draw_cover_frame) {
    draw_list->AddRect(cover_min, cover_max, frame_shadow_color, 0.f,
                       ImDrawFlags_None, frame_width * 2.6f);
    draw_list->AddRect(cover_min, cover_max, frame_color, 0.f, ImDrawFlags_None,
                       frame_width);
  }

  constexpr std::array<ImVec2, 5> kBadgeShape = {
      ImVec2(0.f, 0.f), ImVec2(99.f, 0.f), ImVec2(112.f, 22.f),
      ImVec2(99.f, kDesignHeight), ImVec2(0.f, kDesignHeight)};
  const ImVec2 shadow_origin = ScalePoint(origin, scale, 2.f, 3.f);
  AddConvexPolygon(draw_list, shadow_origin, scale, kBadgeShape,
                   badge_shadow_color);
  AddConvexPolygon(draw_list, origin, scale, kBadgeShape, badge_face_color);
  AddConvexPolygon(
      draw_list, origin, scale,
      std::array<ImVec2, 4>{ImVec2(7.f, 4.f), ImVec2(97.f, 4.f),
                            ImVec2(106.f, 22.f), ImVec2(97.f, 40.f)},
      badge_inset_color);
  AddConvexPolygon(draw_list, origin, scale,
                   std::array<ImVec2, 4>{ImVec2(0.f, 0.f), ImVec2(7.f, 0.f),
                                         ImVec2(7.f, 44.f), ImVec2(0.f, 44.f)},
                   badge_accent_color);
  AddConvexPolygon(draw_list, origin, scale,
                   std::array<ImVec2, 3>{ImVec2(99.f, 0.f), ImVec2(112.f, 22.f),
                                         ImVec2(99.f, 44.f)},
                   badge_accent_dark_color);

  const std::array<ImVec2, 5> badge_outline =
      ScalePoints(origin, scale, kBadgeShape);
  draw_list->AddPolyline(
      badge_outline.data(), static_cast<int>(badge_outline.size()),
      badge_accent_color, ImDrawFlags_Closed, std::max(1.f, 1.2f * scale));
  draw_list->AddLine(ScalePoint(origin, scale, 55.f, 9.f),
                     ScalePoint(origin, scale, 55.f, 35.f),
                     badge_accent_dark_color, std::max(1.f, scale));
  draw_list->AddLine(ScalePoint(origin, scale, 11.f, 40.f),
                     ScalePoint(origin, scale, 97.f, 40.f), badge_accent_color,
                     std::max(1.f, 1.2f * scale));

  const float phase =
      std::fmod(static_cast<float>(animation_timer_.ElapsedInMilliseconds()),
                kAnimationDurationMs) /
      kAnimationDurationMs;
  if (animate && phase <= kSweepEnd) {
    const float sweep_progress = phase / kSweepEnd;
    const float eased_progress = SmoothStep(sweep_progress);
    const float fade =
        std::min({sweep_progress / .16f, (1.f - sweep_progress) / .16f, 1.f});
    const float center_x = -12.f + eased_progress * 113.f;
    const std::array<ImVec2, 4> shine_points = ScalePoints(
        origin, scale,
        std::array<ImVec2, 4>{
            ImVec2(center_x - 5.f, 4.f), ImVec2(center_x + 1.f, 4.f),
            ImVec2(center_x - 13.f, 40.f), ImVec2(center_x - 19.f, 40.f)});
    draw_list->AddConvexPolyFilled(shine_points.data(),
                                   static_cast<int>(shine_points.size()),
                                   WhiteWithAlpha(fade * .42f * opacity));
  }

  DrawHDLabel(draw_list, ScalePoint(origin, scale, 31.f, 22.f), scale,
              primary_text_color);
  DrawEditionLabel(draw_list, origin, scale, secondary_text_color);
  DrawPixelScale(draw_list, origin, scale, badge_accent_color,
                 badge_accent_dark_color);

  draw_list->PopClipRect();
}
