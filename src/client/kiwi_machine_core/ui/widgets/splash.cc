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

#include "ui/widgets/splash.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "build/kiwi_defines.h"
#include "ui/main_window.h"
#include "utility/fonts.h"
#include "utility/localization.h"

namespace {

constexpr int kCloseAnimationDurationMs = 900;
constexpr int kRevealStartMs = 120;
constexpr int kContentFadeDurationMs = 260;
constexpr int kMaxAnimationFrameDeltaMs = 34;
constexpr float kRevealLineWidth = 4.f;
constexpr char kBrandTitle[] = "Kiwi Machine";
constexpr float kPi = 3.14159265358979323846f;

float AnimationProgress(int elapsed_ms, int start_ms, int duration_ms) {
  return std::clamp((elapsed_ms - start_ms) / static_cast<float>(duration_ms),
                    0.f, 1.f);
}

float EaseOutCubic(float progress) {
  const float inverse = 1.f - progress;
  return 1.f - inverse * inverse * inverse;
}

float EaseOutBack(float progress) {
  constexpr float kOvershoot = 1.70158f;
  constexpr float kOvershootPlusOne = kOvershoot + 1.f;
  const float shifted = progress - 1.f;
  return 1.f + kOvershootPlusOne * shifted * shifted * shifted +
         kOvershoot * shifted * shifted;
}

float EaseInOutSine(float progress) {
  return -(std::cos(kPi * progress) - 1.f) / 2.f;
}

ImU32 ColorWithOpacity(int red,
                       int green,
                       int blue,
                       float opacity,
                       int alpha = 255) {
  const int resolved_alpha = static_cast<int>(std::clamp(opacity, 0.f, 1.f) *
                                              static_cast<float>(alpha));
  return IM_COL32(red, green, blue, resolved_alpha);
}

PreferredFontSize GetTitleFontSize(float scale) {
#if KIWI_ANDROID
  return PreferredFontSize::k4x;
#elif KIWI_IOS
  return PreferredFontSize::k3x;
#else
  return scale < .85f ? PreferredFontSize::k1x : PreferredFontSize::k2x;
#endif
}

PreferredFontSize GetStatusFontSize() {
#if KIWI_MOBILE
  return PreferredFontSize::k2x;
#else
  return PreferredFontSize::k1x;
#endif
}

ImVec2 RotateAround(const ImVec2& point, const ImVec2& center, float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  const float x = point.x - center.x;
  const float y = point.y - center.y;
  return ImVec2(center.x + x * cosine - y * sine,
                center.y + x * sine + y * cosine);
}

void DrawRoundedArc(ImDrawList* draw_list,
                    const ImVec2& center,
                    float radius,
                    float start_angle,
                    float sweep,
                    ImU32 color,
                    float thickness) {
  constexpr int kSegments = 12;
  const float end_angle = start_angle + sweep;
  draw_list->PathArcTo(center, radius, start_angle, end_angle, kSegments);
  draw_list->PathStroke(color, 0, thickness);

  const float cap_radius = thickness / 2.f;
  draw_list->AddCircleFilled(ImVec2(center.x + std::cos(start_angle) * radius,
                                    center.y + std::sin(start_angle) * radius),
                             cap_radius, color);
  draw_list->AddCircleFilled(ImVec2(center.x + std::cos(end_angle) * radius,
                                    center.y + std::sin(end_angle) * radius),
                             cap_radius, color);
}

}  // namespace

Splash::Splash(MainWindow* main_window)
    : Widget(main_window), main_window_(main_window) {
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
  set_flags(window_flags);
  set_title("Splash");
}

Splash::~Splash() = default;

int Splash::GetElapsedMs() {
  return first_paint_ ? 0 : timer_.ElapsedInMilliseconds();
}

void Splash::StartClosing() {
  if (closing_)
    return;

  closing_ = true;
  close_animation_elapsed_ms_ = 0;
  closing_timer_.Start();
}

int Splash::GetRemainingCloseAnimationMs() {
  if (!closing_)
    return kCloseAnimationDurationMs;
  return std::max(0, kCloseAnimationDurationMs - close_animation_elapsed_ms_);
}

void Splash::Paint() {
  if (first_paint_) {
    timer_.Start();
    first_paint_ = false;
  }

  const int elapsed_ms = timer_.ElapsedInMilliseconds();
  if (closing_) {
    close_animation_elapsed_ms_ =
        std::min(kCloseAnimationDurationMs,
                 close_animation_elapsed_ms_ +
                     std::min(closing_timer_.ElapsedInMillisecondsAndReset(),
                              kMaxAnimationFrameDeltaMs));
  }
  const int closing_elapsed_ms = close_animation_elapsed_ms_;
  const float close_progress =
      closing_ ? AnimationProgress(closing_elapsed_ms, kRevealStartMs,
                                   kCloseAnimationDurationMs - kRevealStartMs)
               : 0.f;
  const float content_opacity =
      closing_ ? 1.f - EaseInOutSine(AnimationProgress(closing_elapsed_ms, 0,
                                                       kContentFadeDurationMs))
               : 1.f;

  const ImVec2 window_pos = ImGui::GetWindowPos();
  const ImVec2 window_size = ImGui::GetWindowSize();
  const float window_scale =
      std::min(window_size.x / 356.f, window_size.y / 240.f);
  const float scale =
      std::clamp(std::sqrt(std::max(window_scale, 0.f)), .75f, 2.f);
  // Splash is created before the main UI, so its ImGui window can be below
  // windows created later even when Widget z-order is higher. The foreground
  // list keeps the closing mask above the game library while it is revealed.
  ImDrawList* draw_list = ImGui::GetForegroundDrawList();
  const float reveal_target_x =
      window_pos.x +
      std::clamp(static_cast<float>(main_window_->GetMainMenuContentLeft()),
                 0.f, window_size.x);
  const float reveal_progress = EaseInOutSine(close_progress);
  const float reveal_edge_x =
      window_pos.x + window_size.x -
      (window_pos.x + window_size.x - reveal_target_x) * reveal_progress;

  draw_list->PushClipRect(
      window_pos, ImVec2(reveal_edge_x, window_pos.y + window_size.y), true);
  draw_list->AddRectFilled(
      window_pos,
      ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
      ColorWithOpacity(244, 252, 227, 1.f));

  const std::string& loading_text =
      GetLocalizedString(string_resources::IDR_SPLASH_LOADING_GAME_LIBRARY);
  ImFont* title_font = nullptr;
  float title_font_size = 0.f;
  ImVec2 title_text_size;
  {
    ScopedFont font(FontType::kSystemDefault, GetTitleFontSize(scale));
    title_font = font.GetFont();
    title_font_size = font.GetFontSize();
    title_text_size =
        title_font->CalcTextSizeA(title_font_size, FLT_MAX, 0.f, kBrandTitle);
  }

  ImFont* status_font = nullptr;
  float status_font_size = 0.f;
  ImVec2 status_text_size;
  {
    ScopedFont font = GetPreferredFont(
        GetStatusFontSize(), loading_text.c_str(), FontType::kSystemDefault);
    status_font = font.GetFont();
    status_font_size = font.GetFontSize();
    status_text_size = status_font->CalcTextSizeA(status_font_size, FLT_MAX,
                                                  0.f, loading_text.c_str());
  }

  const float base_logo_size =
      std::min({window_size.x * .20f, window_size.y * .28f, 68.f * scale});
  const float final_orbit_radius = base_logo_size / 2.f + 14.f * scale;
  const float title_gap = 9.f * scale;
  const float status_gap = 5.f * scale;
  const float content_height = final_orbit_radius * 2.f + title_gap +
                               title_text_size.y + status_gap +
                               status_text_size.y;
  const float content_top =
      window_pos.y + std::max(0.f, (window_size.y - content_height) / 2.f);
  const ImVec2 center(window_pos.x + window_size.x / 2.f,
                      content_top + final_orbit_radius);

  const float orbit_progress =
      EaseOutCubic(AnimationProgress(elapsed_ms, 100, 600));
  const float orbit_scale = .72f + .28f * orbit_progress;
  const float orbit_radius = final_orbit_radius * orbit_scale;
  const float orbit_opacity = content_opacity * orbit_progress;
  const float orbit_thickness = std::max(2.f, 2.4f * scale);

  draw_list->AddCircle(center, orbit_radius,
                       ColorWithOpacity(174, 187, 181, orbit_opacity), 0,
                       std::max(1.f, 1.7f * scale));

  const float orbit_rotation = elapsed_ms * .00465f - kPi / 2.f;
  constexpr float kArcSweep = .52f;
  const ImU32 arc_color = ColorWithOpacity(95, 144, 51, orbit_opacity);
  DrawRoundedArc(draw_list, center, orbit_radius, orbit_rotation, kArcSweep,
                 arc_color, orbit_thickness);
  DrawRoundedArc(draw_list, center, orbit_radius, orbit_rotation + kPi,
                 kArcSweep, arc_color, orbit_thickness);

  SDL_Texture* logo =
      GetImage(window()->renderer(), image_resources::ImageID::kKiwiMachine);
  if (logo) {
    const float logo_progress = AnimationProgress(elapsed_ms, 280, 580);
    const float logo_easing = EaseOutBack(logo_progress);
    const float logo_size = base_logo_size * (.72f + .28f * logo_easing);
    const float half_size = logo_size / 2.f;
    const float rotation = -.16f * (1.f - logo_easing);
    const ImVec2 top_left = RotateAround(
        ImVec2(center.x - half_size, center.y - half_size), center, rotation);
    const ImVec2 top_right = RotateAround(
        ImVec2(center.x + half_size, center.y - half_size), center, rotation);
    const ImVec2 bottom_right = RotateAround(
        ImVec2(center.x + half_size, center.y + half_size), center, rotation);
    const ImVec2 bottom_left = RotateAround(
        ImVec2(center.x - half_size, center.y + half_size), center, rotation);
    draw_list->AddImageQuad(
        reinterpret_cast<ImTextureID>(logo), top_left, top_right, bottom_right,
        bottom_left, ImVec2(0.f, 0.f), ImVec2(1.f, 0.f), ImVec2(1.f, 1.f),
        ImVec2(0.f, 1.f),
        ColorWithOpacity(255, 255, 255, content_opacity * logo_progress));
  }

  const float final_title_y = center.y + final_orbit_radius + title_gap;
  const float title_progress =
      EaseOutCubic(AnimationProgress(elapsed_ms, 700, 350));
  draw_list->AddText(
      title_font, title_font_size,
      ImVec2(window_pos.x + (window_size.x - title_text_size.x) / 2.f,
             final_title_y + (1.f - title_progress) * 9.f * scale),
      ColorWithOpacity(32, 40, 45, content_opacity * title_progress),
      kBrandTitle);

  const float final_status_y = final_title_y + title_text_size.y + status_gap;
  const float status_progress =
      EaseOutCubic(AnimationProgress(elapsed_ms, 950, 350));
  draw_list->AddText(
      status_font, status_font_size,
      ImVec2(window_pos.x + (window_size.x - status_text_size.x) / 2.f,
             final_status_y + (1.f - status_progress) * 7.f * scale),
      ColorWithOpacity(102, 114, 108, content_opacity * status_progress),
      loading_text.c_str());
  draw_list->PopClipRect();

  if (closing_) {
    const float line_opacity =
        EaseOutCubic(AnimationProgress(closing_elapsed_ms, 0, 100));
    draw_list->AddRectFilled(
        ImVec2(reveal_edge_x - kRevealLineWidth, window_pos.y),
        ImVec2(reveal_edge_x + kRevealLineWidth, window_pos.y + window_size.y),
        ColorWithOpacity(116, 184, 22, line_opacity, 56));
    draw_list->AddRectFilled(
        ImVec2(reveal_edge_x - kRevealLineWidth / 2.f, window_pos.y),
        ImVec2(reveal_edge_x + kRevealLineWidth / 2.f,
               window_pos.y + window_size.y),
        ColorWithOpacity(116, 184, 22, line_opacity));
  }
}

bool Splash::OnKeyPressed(SDL_KeyboardEvent* event) {
  return true;
}

bool Splash::OnKeyReleased(SDL_KeyboardEvent* event) {
  return true;
}

void Splash::OnWindowPreRender() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
}

void Splash::OnWindowPostRender() {
  ImGui::PopStyleVar(2);
}
