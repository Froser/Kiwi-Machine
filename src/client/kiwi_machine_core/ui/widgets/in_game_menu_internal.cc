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

#include "ui/widgets/in_game_menu_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace in_game_menu_internal {

const ImU32 kTextColor = IM_COL32(245, 247, 245, 255);
const ImU32 kMutedTextColor = IM_COL32(169, 176, 169, 255);
const ImU32 kStrongBorderColor = IM_COL32(245, 247, 245, 58);
const ImU32 kBrandColor = IM_COL32(101, 216, 75, 255);
const ImU32 kBrandOnColor = IM_COL32(16, 40, 12, 255);
const float kCornerRadius = 8.f;

namespace {

constexpr float kPi = 3.14159265358979323846f;

}  // namespace

SDL_Rect MakeRect(float x, float y, float width, float height) {
  return SDL_Rect{static_cast<int>(std::round(x)),
                  static_cast<int>(std::round(y)),
                  std::max(0, static_cast<int>(std::round(width))),
                  std::max(0, static_cast<int>(std::round(height)))};
}

SDL_Rect InsetRect(const SDL_Rect& rect, float amount) {
  return MakeRect(rect.x + amount, rect.y + amount, rect.w - amount * 2.f,
                  rect.h - amount * 2.f);
}

float RectRight(const SDL_Rect& rect) {
  return rect.x + rect.w;
}

float RectBottom(const SDL_Rect& rect) {
  return rect.y + rect.h;
}

ImVec2 RectMin(const SDL_Rect& rect) {
  return ImVec2(rect.x, rect.y);
}

ImVec2 RectMax(const SDL_Rect& rect) {
  return ImVec2(RectRight(rect), RectBottom(rect));
}

PreferredFontSize GetSecondaryFontSize(PreferredFontSize size) {
  switch (size) {
    case PreferredFontSize::k6x:
      return PreferredFontSize::k5x;
    case PreferredFontSize::k5x:
      return PreferredFontSize::k4x;
    case PreferredFontSize::k4x:
      return PreferredFontSize::k3x;
    case PreferredFontSize::k3x:
      return PreferredFontSize::k2x;
    case PreferredFontSize::k2x:
    case PreferredFontSize::k1x:
      return PreferredFontSize::k1x;
  }
  return PreferredFontSize::k1x;
}

void DrawTextInRect(const std::string& text,
                    const SDL_Rect& rect,
                    PreferredFontSize font_size,
                    ImU32 color,
                    float horizontal_padding,
                    bool center,
                    bool wrap,
                    FontType default_font,
                    float font_scale) {
  if (rect.w <= 0 || rect.h <= 0 || text.empty())
    return;

  ScopedFont font = GetPreferredFont(font_size, text.c_str(), default_font);
  const float resolved_font_size = font.GetFontSize() * font_scale;
  const float wrap_width =
      wrap ? std::max(1.f, rect.w - horizontal_padding * 2.f) : 0.f;
  ImVec2 text_size = font.GetFont()->CalcTextSizeA(resolved_font_size, FLT_MAX,
                                                   wrap_width, text.c_str());
  float x = rect.x + horizontal_padding;
  if (center)
    x = rect.x + std::max(0.f, (rect.w - text_size.x) / 2.f);
  const float y = rect.y + std::max(0.f, (rect.h - text_size.y) / 2.f);
  ImVec4 clip_rect(rect.x, rect.y, RectRight(rect), RectBottom(rect));
  ImGui::GetWindowDrawList()->AddText(font.GetFont(), resolved_font_size,
                                      ImVec2(x, y), color, text.c_str(),
                                      nullptr, wrap_width, &clip_rect);
}

void DrawChevron(const SDL_Rect& rect, bool points_left, ImU32 color) {
  const float radius = std::min(rect.w, rect.h) * .18f;
  const ImVec2 center(rect.x + rect.w / 2.f, rect.y + rect.h / 2.f);
  if (points_left) {
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        ImVec2(center.x - radius, center.y),
        ImVec2(center.x + radius, center.y - radius),
        ImVec2(center.x + radius, center.y + radius), color);
  } else {
    ImGui::GetWindowDrawList()->AddTriangleFilled(
        ImVec2(center.x + radius, center.y),
        ImVec2(center.x - radius, center.y - radius),
        ImVec2(center.x - radius, center.y + radius), color);
  }
}

void DrawSpinner(const SDL_Rect& rect) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 center(rect.x + rect.w / 2.f, rect.y + rect.h / 2.f);
  const float orbit = std::min(rect.w, rect.h) * .18f;
  const float dot_radius = std::max(2.f, orbit * .16f);
  const float phase = static_cast<float>(ImGui::GetTime()) * 4.f;
  for (int i = 0; i < 8; ++i) {
    const float angle = phase + 2.f * kPi * i / 8.f;
    const int alpha = 64 + i * 23;
    draw_list->AddCircleFilled(ImVec2(center.x + std::cos(angle) * orbit,
                                      center.y + std::sin(angle) * orbit),
                               dot_radius, IM_COL32(101, 216, 75, alpha));
  }
}

}  // namespace in_game_menu_internal
