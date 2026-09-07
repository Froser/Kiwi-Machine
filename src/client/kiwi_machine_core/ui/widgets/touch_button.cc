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

#include "ui/widgets/touch_button.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>

#include "ui/window_base.h"
#include "utility/fonts.h"
#include "utility/images.h"
#include "utility/math.h"

namespace {
constexpr ImU32 kControlFillColor = IM_COL32(24, 29, 27, 255);
constexpr ImU32 kControlBorderColor = IM_COL32(143, 153, 147, 255);
constexpr ImU32 kControlTextColor = IM_COL32(245, 247, 245, 255);
constexpr ImU32 kActionAColor = IM_COL32(240, 82, 82, 255);
constexpr ImU32 kActionBColor = IM_COL32(76, 200, 92, 255);
constexpr ImU32 kActionABColor = IM_COL32(227, 179, 65, 255);

ImU32 WithAlpha(ImU32 color, float alpha) {
  const ImU32 resolved_alpha =
      static_cast<ImU32>(std::clamp(alpha, 0.f, 1.f) * 255.f);
  return (color & ~IM_COL32_A_MASK) | (resolved_alpha << IM_COL32_A_SHIFT);
}

ImU32 GetAccentColor(TouchButton::VisualStyle style) {
  switch (style) {
    case TouchButton::VisualStyle::kActionA:
      return kActionAColor;
    case TouchButton::VisualStyle::kActionB:
      return kActionBColor;
    case TouchButton::VisualStyle::kActionAB:
      return kActionABColor;
    case TouchButton::VisualStyle::kPause:
    case TouchButton::VisualStyle::kImage:
      return kControlBorderColor;
  }
  return kControlBorderColor;
}

const char* GetActionLabel(TouchButton::VisualStyle style) {
  switch (style) {
    case TouchButton::VisualStyle::kActionA:
      return "A";
    case TouchButton::VisualStyle::kActionB:
      return "B";
    case TouchButton::VisualStyle::kActionAB:
      return "A+B";
    case TouchButton::VisualStyle::kPause:
    case TouchButton::VisualStyle::kImage:
      return "";
  }
  return "";
}
}  // namespace

TouchButton::TouchButton(WindowBase* window_base,
                         image_resources::ImageID image_id,
                         VisualStyle visual_style)
    : Widget(window_base), image_id_(image_id), visual_style_(visual_style) {
  SDL_assert(image_id_ != image_resources::ImageID::kLast);
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBackground;
  set_flags(window_flags);
  set_title("##TouchButton");
  SDL_Rect b = bounds();
  if (visual_style_ == VisualStyle::kImage) {
    texture_ = GetImage(window()->renderer(), image_id_);
    SDL_QueryTexture(texture_, nullptr, nullptr, &texture_width_,
                     &texture_height_);
    b.w = texture_width_;
    b.h = texture_height_;
  } else {
    b.w = 64;
    b.h = 64;
  }
  set_bounds(b);
}

TouchButton::~TouchButton() = default;

void TouchButton::Paint() {
  if (visual_style_ != VisualStyle::kImage) {
    const SDL_Rect rect = bounds();
    const bool pressed = button_state_ == ButtonState::kDown;
    const float alpha = pressed ? 1.f : opacity_;
    const float radius = std::min(rect.w, rect.h) * (pressed ? .41f : .46f);
    const ImVec2 center(rect.x + rect.w / 2.f, rect.y + rect.h / 2.f);
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    draw_list->AddCircleFilled(center, radius,
                               WithAlpha(kControlFillColor, alpha * .82f), 48);
    draw_list->AddCircle(center, radius,
                         WithAlpha(GetAccentColor(visual_style_), alpha), 48,
                         std::max(2.f, radius * .065f));

    if (visual_style_ == VisualStyle::kPause) {
      const float bar_height = radius * .72f;
      const float bar_width = std::max(3.f, radius * .14f);
      const float gap = radius * .18f;
      draw_list->AddRectFilled(
          ImVec2(center.x - gap - bar_width, center.y - bar_height / 2.f),
          ImVec2(center.x - gap, center.y + bar_height / 2.f),
          WithAlpha(kControlTextColor, alpha), bar_width / 2.f);
      draw_list->AddRectFilled(
          ImVec2(center.x + gap, center.y - bar_height / 2.f),
          ImVec2(center.x + gap + bar_width, center.y + bar_height / 2.f),
          WithAlpha(kControlTextColor, alpha), bar_width / 2.f);
      return;
    }

    const char* label = GetActionLabel(visual_style_);
    ScopedFont font(
        GetPreferredFont(PreferredFontSize::k4x, FontType::kSystemDefault));
    float font_size = std::min(font.GetFontSize(), radius * .95f);
    ImVec2 text_size =
        font.GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.f, label);
    const float max_text_width = radius * 1.35f;
    if (text_size.x > max_text_width) {
      font_size *= max_text_width / text_size.x;
      text_size = font.GetFont()->CalcTextSizeA(font_size, FLT_MAX, 0.f, label);
    }
    draw_list->AddText(
        font.GetFont(), font_size,
        ImVec2(center.x - text_size.x / 2.f, center.y - text_size.y / 2.f),
        WithAlpha(kControlTextColor, pressed ? 1.f : std::max(.9f, opacity_)),
        label);
    return;
  }

  ImU32 color = button_state_ == ButtonState::kNormal
                    ? IM_COL32(255, 255, 255, opacity_ * 255)
                    : IM_COL32(255, 255, 255, opacity_ * 128);
  SDL_Rect rect = bounds();
  ImGui::GetBackgroundDrawList()->AddImage(
      reinterpret_cast<ImTextureID>(texture_), ImVec2(rect.x, rect.y),
      ImVec2(rect.x + rect.w, rect.y + rect.h), ImVec2(0, 0), ImVec2(1, 1),
      color);
}

bool TouchButton::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  bool handled = false;
  SDL_Rect bounds = window()->GetClientBounds();
  ImVec2 touch_pt(event->x * bounds.w, event->y * bounds.h);
  if (Contains(MapToWindow(this->bounds()), touch_pt.x, touch_pt.y)) {
    triggered_fingers_.insert(std::make_pair(
        event->fingerId, TouchDetail{static_cast<int>(touch_pt.x),
                                     static_cast<int>(touch_pt.y)}));
    if (finger_down_callback_)
      finger_down_callback_.Run();

    handled = true;
  }

  CalculateButtonState();
  return handled;
}

bool TouchButton::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  bool handled = false;
  if (triggered_fingers_.erase(event->fingerId))
    handled = true;

  ButtonState previous_button_state = button_state_;
  CalculateButtonState();

  // If there's no finger on it (button state equals to kNormal), trigger the
  // button.
  if (previous_button_state == ButtonState::kDown &&
      button_state_ == ButtonState::kNormal && visible() && enabled() &&
      trigger_callback_) {
    trigger_callback_.Run();
  }

  return handled;
}

bool TouchButton::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  bool handled = false;
  auto finger_iter = triggered_fingers_.find(event->fingerId);
  if (finger_iter != triggered_fingers_.end()) {
    SDL_Rect bounds = window()->GetClientBounds();
    finger_iter->second.touch_point_x = event->x * bounds.w;
    finger_iter->second.touch_point_y = event->y * bounds.h;
    handled = true;
  }

  CalculateButtonState();
  return handled;
}

int TouchButton::GetHitTestPolicy() {
  return Widget::GetHitTestPolicy() | kAlwaysHitTest;
}

void TouchButton::CalculateButtonState() {
  button_state_ = ButtonState::kNormal;
  for (const auto& finger : triggered_fingers_) {
    if (Contains(MapToWindow(this->bounds()), finger.second.touch_point_x,
                 finger.second.touch_point_y))
      button_state_ = ButtonState::kDown;
  }
}
