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

#include "ui/widgets/about_widget.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <initializer_list>
#include <string>
#include <vector>

#include "ui/main_window.h"
#include "ui/styles.h"
#include "ui/widgets/stack_widget.h"
#include "ui/window_base.h"
#include "utility/audio_effects.h"
#include "utility/images.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"

namespace {

constexpr ImU32 kBackdropColor = IM_COL32(0, 0, 0, 204);
constexpr ImU32 kPanelColor = IM_COL32(18, 22, 18, 246);
constexpr ImU32 kPanelMutedColor = IM_COL32(31, 37, 31, 255);
constexpr ImU32 kTextColor = IM_COL32(246, 248, 246, 255);
constexpr ImU32 kMutedTextColor = IM_COL32(161, 171, 161, 255);
constexpr ImU32 kFaintTextColor = IM_COL32(111, 121, 111, 255);
constexpr ImU32 kBorderColor = IM_COL32(245, 247, 245, 34);
constexpr ImU32 kStrongBorderColor = IM_COL32(245, 247, 245, 62);
constexpr ImU32 kBrandColor = IM_COL32(143, 231, 82, 255);
constexpr ImU32 kBrandSoftColor = IM_COL32(34, 72, 26, 255);
constexpr ImU32 kBrandTextColor = IM_COL32(213, 255, 184, 255);
constexpr ImU32 kKeyColor = IM_COL32(47, 54, 47, 255);
constexpr ImU32 kBadgeColor = IM_COL32(240, 82, 82, 255);
constexpr float kCornerRadius = 8.f;
constexpr char kRepositoryUrl[] = "https://github.com/Froser/Kiwi-Machine";
#if KIWI_MOBILE
constexpr float kTouchMoveThreshold = 12.f;
#endif

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

bool IsValidRect(const SDL_Rect& rect) {
  return rect.w > 0 && rect.h > 0;
}

bool PointInRect(const SDL_Rect& rect, float x, float y) {
  return IsValidRect(rect) && x >= rect.x && x < RectRight(rect) &&
         y >= rect.y && y < RectBottom(rect);
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

PreferredFontSize GetLargerFontSize(PreferredFontSize size) {
  switch (size) {
    case PreferredFontSize::k1x:
      return PreferredFontSize::k2x;
    case PreferredFontSize::k2x:
      return PreferredFontSize::k3x;
    case PreferredFontSize::k3x:
      return PreferredFontSize::k4x;
    case PreferredFontSize::k4x:
      return PreferredFontSize::k5x;
    case PreferredFontSize::k5x:
    case PreferredFontSize::k6x:
      return PreferredFontSize::k6x;
  }
  return size;
}

ImVec2 MeasureText(const std::string& text,
                   PreferredFontSize font_size,
                   FontType default_font = FontType::kDefault) {
  ScopedFont font = GetPreferredFont(font_size, text.c_str(), default_font);
  return font.GetFont()->CalcTextSizeA(font.GetFontSize(), FLT_MAX, 0.f,
                                       text.c_str());
}

void DrawTextInRect(const std::string& text,
                    const SDL_Rect& rect,
                    PreferredFontSize font_size,
                    ImU32 color,
                    float horizontal_padding = 0.f,
                    bool center = false,
                    bool wrap = false,
                    FontType default_font = FontType::kDefault) {
  if (!IsValidRect(rect) || text.empty())
    return;

  ScopedFont font = GetPreferredFont(font_size, text.c_str(), default_font);
  const float wrap_width =
      wrap ? std::max(1.f, rect.w - horizontal_padding * 2.f) : 0.f;
  ImVec2 text_size = font.GetFont()->CalcTextSizeA(font.GetFontSize(), FLT_MAX,
                                                   wrap_width, text.c_str());
  float x = rect.x + horizontal_padding;
  if (center)
    x = rect.x + std::max(0.f, (rect.w - text_size.x) / 2.f);
  const float y = rect.y + std::max(0.f, (rect.h - text_size.y) / 2.f);
  const ImVec4 clip_rect(rect.x, rect.y, RectRight(rect), RectBottom(rect));
  ImGui::GetWindowDrawList()->AddText(font.GetFont(), font.GetFontSize(),
                                      ImVec2(x, y), color, text.c_str(),
                                      nullptr, wrap_width, &clip_rect);
}

float MeasureWrappedTextHeight(const std::string& text,
                               PreferredFontSize font_size,
                               float width,
                               FontType default_font = FontType::kDefault) {
  ScopedFont font = GetPreferredFont(font_size, text.c_str(), default_font);
  return font.GetFont()
      ->CalcTextSizeA(font.GetFontSize(), FLT_MAX, std::max(1.f, width),
                      text.c_str())
      .y;
}

void DrawWrappedTextAtTop(const std::string& text,
                          const SDL_Rect& rect,
                          PreferredFontSize font_size,
                          ImU32 color,
                          FontType default_font = FontType::kDefault) {
  if (!IsValidRect(rect) || text.empty())
    return;

  ScopedFont font = GetPreferredFont(font_size, text.c_str(), default_font);
  const float wrap_width = std::max(1.f, static_cast<float>(rect.w));
  const ImVec4 clip_rect(rect.x, rect.y, RectRight(rect), RectBottom(rect));
  ImGui::GetWindowDrawList()->AddText(font.GetFont(), font.GetFontSize(),
                                      RectMin(rect), color, text.c_str(),
                                      nullptr, wrap_width, &clip_rect);
}

void DrawTextAt(const std::string& text,
                const ImVec2& position,
                PreferredFontSize font_size,
                ImU32 color,
                FontType default_font = FontType::kDefault) {
  if (text.empty())
    return;
  ScopedFont font = GetPreferredFont(font_size, text.c_str(), default_font);
  ImGui::GetWindowDrawList()->AddText(font.GetFont(), font.GetFontSize(),
                                      position, color, text.c_str());
}

void DrawPanel(const SDL_Rect& rect) {
  if (!IsValidRect(rect))
    return;
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(RectMin(rect), RectMax(rect), kPanelColor,
                           kCornerRadius);
  draw_list->AddRect(RectMin(rect), RectMax(rect), kBorderColor, kCornerRadius);
}

void DrawSectionTitle(const std::string& text,
                      const SDL_Rect& rect,
                      PreferredFontSize font_size,
                      float scale) {
  if (!IsValidRect(rect))
    return;
  const float bar_width = std::max(3.f, 3.f * scale);
  const float bar_height = std::min(rect.h * .52f, 22.f * scale);
  const float bar_y = rect.y + (rect.h - bar_height) / 2.f;
  ImGui::GetWindowDrawList()->AddRectFilled(
      ImVec2(rect.x, bar_y), ImVec2(rect.x + bar_width, bar_y + bar_height),
      kBrandColor, bar_width / 2.f);
  SDL_Rect text_rect = MakeRect(rect.x + bar_width + 9.f * scale, rect.y,
                                rect.w - bar_width - 9.f * scale, rect.h);
  DrawTextInRect(text, text_rect, font_size, kTextColor);
}

void DrawKeycap(const std::string& text,
                const SDL_Rect& rect,
                PreferredFontSize font_size,
                bool accent) {
  if (!IsValidRect(rect))
    return;
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImU32 fill = accent ? kBrandSoftColor : kKeyColor;
  const ImU32 border = accent ? kBrandColor : kStrongBorderColor;
  draw_list->AddRectFilled(RectMin(rect), RectMax(rect), fill, 5.f);
  draw_list->AddRect(RectMin(rect), RectMax(rect), border, 5.f);
  draw_list->AddLine(ImVec2(rect.x + 4.f, RectBottom(rect) - 2.f),
                     ImVec2(RectRight(rect) - 4.f, RectBottom(rect) - 2.f),
                     accent ? kBrandColor : kBorderColor, 1.f);
  DrawTextInRect(text, rect, font_size, accent ? kBrandTextColor : kTextColor,
                 2.f, true, false, FontType::kSystemDefault);
}

void DrawKeyGroup(const SDL_Rect& rect,
                  PreferredFontSize font_size,
                  float scale,
                  std::initializer_list<const char*> keys,
                  bool accent = false) {
  if (!IsValidRect(rect) || keys.size() == 0)
    return;

  const float gap = 4.f * scale;
  const float key_height =
      std::min(static_cast<float>(rect.h), std::max(28.f * scale, 20.f));
  std::vector<float> widths;
  widths.reserve(keys.size());
  float total_width = gap * static_cast<float>(keys.size() - 1);
  for (const char* key : keys) {
    const float width = std::max(
        28.f * scale,
        MeasureText(key, font_size, FontType::kSystemDefault).x + 12.f * scale);
    widths.push_back(width);
    total_width += width;
  }

  if (total_width > rect.w) {
    const float available =
        std::max(1.f, rect.w - gap * static_cast<float>(keys.size() - 1));
    float width_sum = total_width - gap * static_cast<float>(keys.size() - 1);
    const float shrink = available / std::max(1.f, width_sum);
    total_width = gap * static_cast<float>(keys.size() - 1);
    for (float& width : widths) {
      width *= shrink;
      total_width += width;
    }
  }

  float x = rect.x + std::max(0.f, (rect.w - total_width) / 2.f);
  const float y = rect.y + std::max(0.f, (rect.h - key_height) / 2.f);
  size_t index = 0;
  for (const char* key : keys) {
    DrawKeycap(key, MakeRect(x, y, widths[index], key_height), font_size,
               accent);
    x += widths[index] + gap;
    ++index;
  }
}

void DrawControllerIllustration(const SDL_Rect& rect, float scale) {
  if (!IsValidRect(rect))
    return;

  const float width = std::min(rect.w * .82f, rect.h * 1.65f);
  const float height = std::min(rect.h * .68f, width * .55f);
  const float x = rect.x + (rect.w - width) / 2.f;
  const float y = rect.y + (rect.h - height) / 2.f;
  const SDL_Rect body = MakeRect(x, y, width, height);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(RectMin(body), RectMax(body), kPanelMutedColor,
                           std::min(28.f * scale, height * .3f));
  draw_list->AddRect(RectMin(body), RectMax(body), kStrongBorderColor,
                     std::min(28.f * scale, height * .3f), 0, 2.f);

  const ImVec2 dpad_center(x + width * .27f, y + height * .5f);
  const float dpad_long = height * .48f;
  const float dpad_short = height * .16f;
  draw_list->AddRectFilled(
      ImVec2(dpad_center.x - dpad_long / 2.f, dpad_center.y - dpad_short / 2.f),
      ImVec2(dpad_center.x + dpad_long / 2.f, dpad_center.y + dpad_short / 2.f),
      kMutedTextColor, 3.f);
  draw_list->AddRectFilled(
      ImVec2(dpad_center.x - dpad_short / 2.f, dpad_center.y - dpad_long / 2.f),
      ImVec2(dpad_center.x + dpad_short / 2.f, dpad_center.y + dpad_long / 2.f),
      kMutedTextColor, 3.f);

  const float face_radius = std::max(5.f, height * .095f);
  const ImVec2 face_a(x + width * .73f, y + height * .41f);
  const ImVec2 face_b(x + width * .84f, y + height * .58f);
  draw_list->AddCircleFilled(face_a, face_radius, kBrandColor);
  draw_list->AddCircleFilled(face_b, face_radius, kBadgeColor);
  draw_list->AddCircle(face_a, face_radius, kStrongBorderColor, 0, 2.f);
  draw_list->AddCircle(face_b, face_radius, kStrongBorderColor, 0, 2.f);

  const float center_y = y + height * .68f;
  draw_list->AddRectFilled(
      ImVec2(x + width * .43f, center_y),
      ImVec2(x + width * .50f, center_y + std::max(4.f, height * .055f)),
      kFaintTextColor, 3.f);
  draw_list->AddRectFilled(
      ImVec2(x + width * .53f, center_y),
      ImVec2(x + width * .60f, center_y + std::max(4.f, height * .055f)),
      kFaintTextColor, 3.f);
}

void DrawBackArrow(const SDL_Rect& rect, ImU32 color, float scale) {
  if (!IsValidRect(rect))
    return;
  const ImVec2 center(rect.x + rect.w / 2.f, rect.y + rect.h / 2.f);
  const float arm = std::min(rect.w, rect.h) * .18f;
  const float shaft = std::min(rect.w, rect.h) * .24f;
  const float thickness = std::max(1.5f, 1.5f * scale);
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddLine(ImVec2(center.x - arm, center.y),
                     ImVec2(center.x + shaft, center.y), color, thickness);
  draw_list->AddLine(ImVec2(center.x - arm, center.y),
                     ImVec2(center.x, center.y - arm), color, thickness);
  draw_list->AddLine(ImVec2(center.x - arm, center.y),
                     ImVec2(center.x, center.y + arm), color, thickness);
}

}  // namespace

AboutWidget::AboutWidget(MainWindow* main_window,
                         StackWidget* parent,
                         NESRuntimeID runtime_id)
    : Widget(main_window), parent_(parent), main_window_(main_window) {
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("About");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
}

AboutWidget::~AboutWidget() = default;

void AboutWidget::Close() {
  SDL_assert(parent_);
  mouse_pressed_ = false;
#if KIWI_MOBILE
  touch_active_ = false;
#endif
  parent_->PopWidget();
}

void AboutWidget::Paint() {
  FrameText text = BuildFrameText();
  FrameLayout layout = CalculateFrameLayout(text);
  last_layout_ = layout;
  has_layout_ = true;
  DrawFrame(text, layout);
}

void AboutWidget::OnWindowResized() {
  set_bounds(window()->GetClientBounds());
  has_layout_ = false;
}

bool AboutWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvent(event, nullptr);
}

bool AboutWidget::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvent(nullptr, event);
}

bool AboutWidget::OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) {
  static_cast<void>(event);
  return HandleInputEvent(nullptr, nullptr);
}

void AboutWidget::OnWindowPreRender() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
}

void AboutWidget::OnWindowPostRender() {
  ImGui::PopStyleVar(2);
}

bool AboutWidget::OnMouseMove(SDL_MouseMotionEvent* event) {
  if (has_layout_)
    hovered_target_ = HitTest(last_layout_, event->x, event->y);
  return true;
}

bool AboutWidget::OnMousePressed(SDL_MouseButtonEvent* event) {
  if (!has_layout_ || event->button != SDL_BUTTON_LEFT)
    return true;
  pressed_target_ = HitTest(last_layout_, event->x, event->y);
  mouse_pressed_ = pressed_target_ != HitTarget::kNone;
  return true;
}

bool AboutWidget::OnMouseReleased(SDL_MouseButtonEvent* event) {
  if (event->button == SDL_BUTTON_RIGHT) {
    PlayEffect(audio_resources::AudioID::kBack);
    Close();
    return true;
  }
  if (event->button != SDL_BUTTON_LEFT)
    return true;

  HitTarget released_target = has_layout_
                                  ? HitTest(last_layout_, event->x, event->y)
                                  : HitTarget::kNone;
  if (mouse_pressed_ && released_target == pressed_target_)
    ActivateHitTarget(released_target);
  mouse_pressed_ = false;
  pressed_target_ = HitTarget::kNone;
  return true;
}

#if KIWI_MOBILE
bool AboutWidget::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  if (!has_layout_)
    return true;
  SDL_Rect window_bounds = window()->GetClientBounds();
  ImVec2 point(event->x * window_bounds.w, event->y * window_bounds.h);
  touch_active_ = true;
  active_touch_id_ = event->fingerId;
  touch_down_position_ = point;
  touch_moved_ = false;
  pressed_target_ = HitTest(last_layout_, point.x, point.y);
  return true;
}

bool AboutWidget::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  if (!touch_active_ || event->fingerId != active_touch_id_)
    return true;
  SDL_Rect window_bounds = window()->GetClientBounds();
  ImVec2 point(event->x * window_bounds.w, event->y * window_bounds.h);
  const float delta_x = point.x - touch_down_position_.x;
  const float delta_y = point.y - touch_down_position_.y;
  if (std::sqrt(delta_x * delta_x + delta_y * delta_y) > kTouchMoveThreshold) {
    touch_moved_ = true;
  }
  return true;
}

bool AboutWidget::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  if (!touch_active_ || event->fingerId != active_touch_id_)
    return true;

  SDL_Rect window_bounds = window()->GetClientBounds();
  ImVec2 point(event->x * window_bounds.w, event->y * window_bounds.h);
  HitTarget released_target = HitTest(last_layout_, point.x, point.y);
  if (!touch_moved_ && released_target == pressed_target_) {
    if (released_target == HitTarget::kNone) {
      PlayEffect(audio_resources::AudioID::kBack);
      Close();
    } else {
      ActivateHitTarget(released_target);
    }
  }

  touch_active_ = false;
  touch_moved_ = false;
  pressed_target_ = HitTarget::kNone;
  return true;
}
#endif

AboutWidget::FrameText AboutWidget::BuildFrameText() const {
  using namespace string_resources;
  FrameText text;
  text.title = GetLocalizedString(IDR_ABOUT_KIWI_MACHINE);
  text.subtitle = GetLocalizedString(IDR_ABOUT_INSTRUCTIONS);
  text.version = "VERSION 2.0.0";
  text.short_version = "2.0.0";
  text.controller = GetLocalizedString(IDR_ABOUT_CONTROLLER);
  text.keyboard = GetLocalizedString(IDR_ABOUT_CONTROLLER_KEYBOARD);
  text.gamepad = GetLocalizedString(IDR_ABOUT_CONTROLLER_GAMEPAD);
  text.input = GetLocalizedString(IDR_ABOUT_CONTROLLER_INPUT);
  text.direction = GetLocalizedString(IDR_ABOUT_CONTROLLER_DIRECTION);
  text.button_a = GetLocalizedString(IDR_ABOUT_CONTROLLER_A);
  text.button_b = GetLocalizedString(IDR_ABOUT_CONTROLLER_B);
  text.select = GetLocalizedString(IDR_ABOUT_CONTROLLER_SELECT);
  text.start = GetLocalizedString(IDR_ABOUT_CONTROLLER_START);
  text.menu = GetLocalizedString(IDR_ABOUT_CONTROLLER_INVOKE_MENU);
  text.player_one = GetLocalizedString(IDR_ABOUT_CONTROLLER_KEYBOARD_1);
  text.player_two = GetLocalizedString(IDR_ABOUT_CONTROLLER_KEYBOARD_2);
  text.xbox = GetLocalizedString(IDR_ABOUT_CONTROLLER_XBOX);
  text.xbox_direction = GetLocalizedString(IDR_ABOUT_CONTROLLER_XBOX_DIRECTION);
  text.xbox_menu = GetLocalizedString(IDR_ABOUT_CONTROLLER_XBOX_MENU);
  text.game_selection = GetLocalizedString(IDR_ABOUT_GAME_SELECTION);
  text.game_selection_description = {
      GetLocalizedString(IDR_ABOUT_GAME_SELECTION_CHANGE_VERSION_0),
      GetLocalizedString(IDR_ABOUT_GAME_SELECTION_CHANGE_VERSION_1),
      GetLocalizedString(IDR_ABOUT_GAME_SELECTION_CHANGE_VERSION_2),
  };
  text.about = GetLocalizedString(IDR_ABOUT_ABOUT);
  text.repository_label = "Froser / Kiwi-Machine";
  text.author = GetLocalizedString(IDR_ABOUT_ABOUT_AUTHOR);
  return text;
}

AboutWidget::FrameLayout AboutWidget::CalculateFrameLayout(
    const FrameText& text) const {
  FrameLayout layout;
  layout.title_font = styles::about_widget::PreferredTitleFontSize(
      main_window_->window_scale());
  layout.body_font = styles::about_widget::PreferredContentFontSize();
  if (layout.title_font == layout.body_font)
    layout.title_font = GetLargerFontSize(layout.title_font);
  {
    ScopedFont font =
        GetPreferredFont(layout.body_font, text.controller.c_str());
    layout.font_height = font.GetFontSize();
  }
  layout.scale = std::clamp(layout.font_height / 16.f, 1.f, 2.f);
  layout.padding = 16.f * layout.scale;

  SDL_Rect safe_area_insets = main_window_->GetSafeAreaInsets();
  ImVec2 window_pos = ImGui::GetWindowPos();
  ImVec2 window_size = ImGui::GetWindowSize();
  layout.safe_area = MakeRect(
      window_pos.x + safe_area_insets.x, window_pos.y + safe_area_insets.y,
      window_size.x - safe_area_insets.x - safe_area_insets.w,
      window_size.y - safe_area_insets.y - safe_area_insets.h);

  SDL_Rect workspace = layout.safe_area;
  const float outer_margin = std::clamp(20.f * layout.scale, 16.f, 32.f);
  layout.page = InsetRect(workspace, outer_margin);

  const float max_page_width = 1180.f * layout.scale;
  if (layout.page.w > max_page_width) {
    layout.page.x += static_cast<int>((layout.page.w - max_page_width) / 2.f);
    layout.page.w = static_cast<int>(max_page_width);
  }
  const bool compact_header = layout.page.w < 540.f * layout.scale;

#if KIWI_MOBILE
  layout.mode = LayoutMode::kSingleColumn;
#else
  layout.mode = layout.page.w >= 620.f * layout.scale &&
                        layout.page.w > layout.page.h * .95f
                    ? LayoutMode::kTwoColumn
                    : LayoutMode::kSingleColumn;
#endif

  const float header_height =
      std::min(96.f * layout.scale, layout.page.h * .22f);
  layout.header =
      MakeRect(layout.page.x, layout.page.y, layout.page.w, header_height);
  const float logo_size = std::max(
      0.f, std::min({68.f * layout.scale, layout.header.h - 12.f * layout.scale,
                     layout.header.w * .16f}));
  layout.logo = MakeRect(layout.header.x,
                         layout.header.y + (layout.header.h - logo_size) / 2.f,
                         logo_size, logo_size);

  const std::string& version_label =
      compact_header ? text.short_version : text.version;
  const float desired_version_width = std::max(
      (compact_header ? 54.f : 112.f) * layout.scale,
      MeasureText(version_label, GetSecondaryFontSize(layout.body_font),
                  FontType::kSystemDefault)
              .x +
          36.f * layout.scale);
  const float version_width =
      std::min(desired_version_width, layout.header.w * .32f);
  const float back_button_size = 30.f * layout.scale;
  layout.back_button =
      MakeRect(RectRight(layout.header) - back_button_size,
               layout.header.y + (layout.header.h - back_button_size) / 2.f,
               back_button_size, back_button_size);
  layout.version =
      MakeRect(layout.back_button.x - 8.f * layout.scale - version_width,
               layout.header.y + (layout.header.h - 30.f * layout.scale) / 2.f,
               version_width, 30.f * layout.scale);

  const float text_left = RectRight(layout.logo) + 14.f * layout.scale;
  const float text_right = layout.version.x - 14.f * layout.scale;
  if (MeasureText(text.title, layout.title_font).x > text_right - text_left) {
    layout.title_font = layout.body_font;
    if (MeasureText(text.title, layout.title_font).x > text_right - text_left) {
      layout.title_font = GetSecondaryFontSize(layout.body_font);
    }
  }
  const float title_height =
      MeasureText(text.title, layout.title_font).y + 2.f * layout.scale;
  const float subtitle_height =
      MeasureText(text.subtitle, GetSecondaryFontSize(layout.body_font)).y +
      2.f * layout.scale;
  const float text_top =
      layout.header.y +
      (layout.header.h - title_height - subtitle_height) / 2.f;
  layout.title =
      MakeRect(text_left, text_top, text_right - text_left, title_height);
  layout.subtitle = MakeRect(text_left, RectBottom(layout.title),
                             text_right - text_left, subtitle_height);

  const float gap = 14.f * layout.scale;
  SDL_Rect body =
      MakeRect(layout.page.x, RectBottom(layout.header) + gap, layout.page.w,
               RectBottom(layout.page) - RectBottom(layout.header) - gap);
#if KIWI_MOBILE
  const float section_tab_gap = 8.f * layout.scale;
  const float section_bar_height = std::min(44.f * layout.scale, body.h * .2f);
  layout.mobile_section_bar =
      MakeRect(body.x, body.y, body.w, section_bar_height);
  const float section_tab_width =
      (layout.mobile_section_bar.w - section_tab_gap * 2.f) / 3.f;
  for (size_t i = 0; i < layout.mobile_section_tabs.size(); ++i) {
    layout.mobile_section_tabs[i] = MakeRect(
        layout.mobile_section_bar.x + i * (section_tab_width + section_tab_gap),
        layout.mobile_section_bar.y, section_tab_width,
        layout.mobile_section_bar.h);
  }

  SDL_Rect mobile_content =
      MakeRect(body.x, RectBottom(layout.mobile_section_bar) + gap, body.w,
               RectBottom(body) - RectBottom(layout.mobile_section_bar) - gap);
  switch (mobile_section_) {
    case MobileSection::kControls:
      layout.controls_panel = mobile_content;
      break;
    case MobileSection::kGameSelection:
      layout.game_selection_panel = mobile_content;
      break;
    case MobileSection::kAbout:
      layout.about_panel = mobile_content;
      break;
  }
#else
  if (layout.mode == LayoutMode::kTwoColumn) {
    const float side_width =
        std::clamp(body.w * .3f, 250.f * layout.scale, 340.f * layout.scale);
    const float content_height =
        std::min(static_cast<float>(body.h), 290.f * layout.scale);
    layout.controls_panel =
        MakeRect(body.x, body.y, body.w - side_width - gap, content_height);
    const float selection_height = (content_height - gap) * .5f;
    layout.game_selection_panel =
        MakeRect(RectRight(layout.controls_panel) + gap, body.y, side_width,
                 selection_height);
    layout.about_panel =
        MakeRect(layout.game_selection_panel.x,
                 RectBottom(layout.game_selection_panel) + gap, side_width,
                 content_height - selection_height - gap);
  } else {
    const float controls_height = body.h * .5f;
    layout.controls_panel = MakeRect(body.x, body.y, body.w, controls_height);
    const float remaining_height = body.h - controls_height - gap * 2.f;
    const float selection_height = remaining_height * .46f;
    layout.game_selection_panel =
        MakeRect(body.x, RectBottom(layout.controls_panel) + gap, body.w,
                 selection_height);
    layout.about_panel =
        MakeRect(body.x, RectBottom(layout.game_selection_panel) + gap, body.w,
                 remaining_height - selection_height);
  }
#endif

  const float panel_header_height =
      std::min(48.f * layout.scale, layout.controls_panel.h * .24f);
  const float available_tab_width =
      std::max(0.f, layout.controls_panel.w - layout.padding * 2.f);
  const float tab_width =
      std::min(68.f * layout.scale, available_tab_width / 2.f);
  const float tab_height =
      std::min(30.f * layout.scale,
               std::max(0.f, panel_header_height - 8.f * layout.scale));
  layout.gamepad_tab = MakeRect(
      RectRight(layout.controls_panel) - layout.padding - tab_width,
      layout.controls_panel.y + (panel_header_height - tab_height) / 2.f,
      tab_width, tab_height);
  layout.keyboard_tab = MakeRect(layout.gamepad_tab.x - tab_width,
                                 layout.gamepad_tab.y, tab_width, tab_height);
  layout.controls_title = MakeRect(
      layout.controls_panel.x + layout.padding, layout.controls_panel.y,
      layout.keyboard_tab.x - layout.controls_panel.x - layout.padding * 2.f,
      panel_header_height);
  layout.controls_body =
      MakeRect(layout.controls_panel.x + layout.padding,
               layout.controls_panel.y + panel_header_height,
               layout.controls_panel.w - layout.padding * 2.f,
               layout.controls_panel.h - panel_header_height - layout.padding);

  const float game_selection_title_height =
      std::min(42.f * layout.scale, layout.game_selection_panel.h * .34f);
  layout.game_selection_title =
      MakeRect(layout.game_selection_panel.x + layout.padding,
               layout.game_selection_panel.y,
               layout.game_selection_panel.w - layout.padding * 2.f,
               game_selection_title_height);
  layout.game_selection_body =
      MakeRect(layout.game_selection_panel.x + layout.padding,
               RectBottom(layout.game_selection_title),
               layout.game_selection_panel.w - layout.padding * 2.f,
               layout.game_selection_panel.h - game_selection_title_height -
                   layout.padding);
  const float about_title_height =
      std::min(42.f * layout.scale, layout.about_panel.h * .26f);
  layout.about_title =
      MakeRect(layout.about_panel.x + layout.padding, layout.about_panel.y,
               layout.about_panel.w - layout.padding * 2.f, about_title_height);
  const float about_available_height =
      std::max(0.f, layout.about_panel.h - about_title_height - layout.padding);
  const float repository_height =
      std::min(36.f * layout.scale, about_available_height * .34f);
  const float metadata_height =
      std::min(24.f * layout.scale, about_available_height * .22f);
  const float repository_gap =
      std::min(8.f * layout.scale, about_available_height * .08f);
  layout.metadata = MakeRect(
      layout.about_panel.x + layout.padding,
      RectBottom(layout.about_panel) - layout.padding - metadata_height,
      layout.about_panel.w - layout.padding * 2.f, metadata_height);
  layout.repository =
      MakeRect(layout.about_panel.x + layout.padding,
               layout.metadata.y - repository_gap - repository_height,
               layout.about_panel.w - layout.padding * 2.f, repository_height);
  layout.about_body = MakeRect(
      layout.about_panel.x + layout.padding, RectBottom(layout.about_title),
      layout.about_panel.w - layout.padding * 2.f,
      layout.repository.y - RectBottom(layout.about_title) - repository_gap);
  return layout;
}

void AboutWidget::DrawFrame(const FrameText& text, const FrameLayout& layout) {
  DrawBackground(layout);
  DrawHeader(text, layout);
#if KIWI_MOBILE
  DrawMobileSectionTabs(text, layout);
  switch (mobile_section_) {
    case MobileSection::kControls:
      DrawControls(text, layout);
      break;
    case MobileSection::kGameSelection:
      DrawGameSelection(text, layout);
      break;
    case MobileSection::kAbout:
      DrawAbout(text, layout);
      break;
  }
#else
  DrawControls(text, layout);
  DrawGameSelection(text, layout);
  DrawAbout(text, layout);
#endif

  ImGui::SetCursorScreenPos(RectMax(layout.page));
  ImGui::Dummy(ImVec2(0.f, 0.f));
}

#if KIWI_MOBILE
void AboutWidget::DrawMobileSectionTabs(const FrameText& text,
                                        const FrameLayout& layout) {
  const std::array<std::string, 3> labels = {
      text.controller,
      text.game_selection,
      text.about,
  };
  const std::array<HitTarget, 3> targets = {
      HitTarget::kControlsSection,
      HitTarget::kGameSelectionSection,
      HitTarget::kAboutSection,
  };
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  for (size_t i = 0; i < labels.size(); ++i) {
    const bool selected = i == static_cast<size_t>(mobile_section_);
    const bool hovered = hovered_target_ == targets[i];
    const SDL_Rect& rect = layout.mobile_section_tabs[i];
    draw_list->AddRectFilled(
        RectMin(rect), RectMax(rect),
        selected ? kBrandSoftColor
                 : (hovered ? kPanelMutedColor : IM_COL32(0, 0, 0, 0)),
        6.f);
    draw_list->AddRect(RectMin(rect), RectMax(rect),
                       selected ? kBrandColor : kBorderColor, 6.f);
    DrawTextInRect(labels[i], rect, GetSecondaryFontSize(layout.body_font),
                   selected ? kBrandTextColor : kMutedTextColor, 4.f, true,
                   false);
  }
}
#endif

void AboutWidget::DrawBackground(const FrameLayout& layout) {
  static_cast<void>(layout);
  ImVec2 window_pos = ImGui::GetWindowPos();
  ImVec2 window_size = ImGui::GetWindowSize();
  ImGui::GetWindowDrawList()->AddRectFilled(
      window_pos,
      ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
      kBackdropColor);
}

void AboutWidget::DrawHeader(const FrameText& text, const FrameLayout& layout) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddLine(
      ImVec2(layout.header.x, RectBottom(layout.header)),
      ImVec2(RectRight(layout.header), RectBottom(layout.header)),
      kBorderColor);

  if (IsValidRect(layout.logo)) {
    SDL_Texture* logo =
        GetImage(window()->renderer(), image_resources::ImageID::kKiwiMachine);
    draw_list->AddImage(reinterpret_cast<ImTextureID>(logo),
                        RectMin(layout.logo), RectMax(layout.logo));
  }
  DrawTextInRect(text.title, layout.title, layout.title_font, kTextColor);
  DrawTextInRect(text.subtitle, layout.subtitle,
                 GetSecondaryFontSize(layout.body_font), kMutedTextColor);

  draw_list->AddRectFilled(RectMin(layout.version), RectMax(layout.version),
                           kBrandSoftColor, 6.f);
  draw_list->AddRect(RectMin(layout.version), RectMax(layout.version),
                     kBrandColor, 6.f);
  const float dot_radius = std::max(3.f, 3.f * layout.scale);
  const ImVec2 dot(layout.version.x + 12.f * layout.scale,
                   layout.version.y + layout.version.h / 2.f);
  draw_list->AddCircleFilled(dot, dot_radius, kBrandColor);
  SDL_Rect version_text =
      MakeRect(dot.x + 8.f * layout.scale, layout.version.y,
               RectRight(layout.version) - dot.x - 12.f * layout.scale,
               layout.version.h);
  const std::string& version_label =
      layout.page.w < 540.f * layout.scale ? text.short_version : text.version;
  DrawTextInRect(version_label, version_text,
                 GetSecondaryFontSize(layout.body_font), kBrandTextColor, 0.f,
                 false, false, FontType::kSystemDefault);

  const bool back_hovered = hovered_target_ == HitTarget::kBack;
  draw_list->AddRectFilled(
      RectMin(layout.back_button), RectMax(layout.back_button),
      back_hovered ? kBrandSoftColor : kPanelMutedColor, 6.f);
  draw_list->AddRect(RectMin(layout.back_button), RectMax(layout.back_button),
                     back_hovered ? kBrandColor : kStrongBorderColor, 6.f);
  DrawBackArrow(layout.back_button, back_hovered ? kBrandTextColor : kTextColor,
                layout.scale);
}

void AboutWidget::DrawControls(const FrameText& text,
                               const FrameLayout& layout) {
  DrawPanel(layout.controls_panel);
  DrawSectionTitle(text.controller, layout.controls_title, layout.body_font,
                   layout.scale);

  auto draw_tab = [this, &layout](const std::string& label,
                                  const SDL_Rect& rect, InputPage page,
                                  HitTarget target) {
    const bool selected = input_page_ == page;
    const bool hovered = hovered_target_ == target;
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(
        RectMin(rect), RectMax(rect),
        selected ? kBrandSoftColor
                 : (hovered ? kPanelMutedColor : IM_COL32(0, 0, 0, 0)),
        5.f);
    draw_list->AddRect(RectMin(rect), RectMax(rect),
                       selected ? kBrandColor : kBorderColor, 5.f);
    DrawTextInRect(label, rect, GetSecondaryFontSize(layout.body_font),
                   selected ? kBrandTextColor : kMutedTextColor, 2.f, true);
  };
  draw_tab(text.keyboard, layout.keyboard_tab, InputPage::kKeyboard,
           HitTarget::kKeyboardTab);
  draw_tab(text.gamepad, layout.gamepad_tab, InputPage::kGamepad,
           HitTarget::kGamepadTab);

  if (input_page_ == InputPage::kKeyboard)
    DrawKeyboard(text, layout);
  else
    DrawGamepad(text, layout);
}

void AboutWidget::DrawKeyboard(const FrameText& text,
                               const FrameLayout& layout) {
  const SDL_Rect& body = layout.controls_body;
  if (!IsValidRect(body))
    return;
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddLine(RectMin(body), ImVec2(RectRight(body), body.y),
                     kBorderColor);

  const std::array<std::string, 4> player_one_directions = {"W", "A", "S", "D"};
  const std::array<std::string, 4> player_two_directions = {
      GetLocalizedString(string_resources::IDR_ABOUT_CONTROLLER_KEY_UP),
      GetLocalizedString(string_resources::IDR_ABOUT_CONTROLLER_KEY_LEFT),
      GetLocalizedString(string_resources::IDR_ABOUT_CONTROLLER_KEY_DOWN),
      GetLocalizedString(string_resources::IDR_ABOUT_CONTROLLER_KEY_RIGHT),
  };

  auto draw_wide_row = [&layout](const SDL_Rect& row, const std::string& player,
                                 const std::array<std::string, 4>& directions,
                                 const char* button_a, const char* button_b,
                                 const char* select, const char* start) {
    constexpr std::array<float, 6> kColumnFractions = {.16f, .30f, .11f,
                                                       .11f, .15f, .17f};
    std::array<SDL_Rect, 6> columns;
    float x = row.x;
    for (size_t i = 0; i < columns.size(); ++i) {
      const float right = i + 1 == columns.size()
                              ? RectRight(row)
                              : x + row.w * kColumnFractions[i];
      columns[i] = MakeRect(x, row.y, right - x, row.h);
      x = right;
    }

    DrawTextInRect(player, columns[0], layout.body_font, kTextColor,
                   6.f * layout.scale);
    DrawKeyGroup(columns[1], GetSecondaryFontSize(layout.body_font),
                 layout.scale,
                 {directions[0].c_str(), directions[1].c_str(),
                  directions[2].c_str(), directions[3].c_str()});
    DrawKeyGroup(columns[2], GetSecondaryFontSize(layout.body_font),
                 layout.scale, {button_a}, true);
    DrawKeyGroup(columns[3], GetSecondaryFontSize(layout.body_font),
                 layout.scale, {button_b});
    DrawKeyGroup(columns[4], GetSecondaryFontSize(layout.body_font),
                 layout.scale, {select});
    DrawKeyGroup(columns[5], GetSecondaryFontSize(layout.body_font),
                 layout.scale, {start});
  };

  if (body.w >= 560.f * layout.scale) {
    const float header_height = std::min(30.f * layout.scale, body.h * .2f);
    const SDL_Rect header = MakeRect(body.x, body.y, body.w, header_height);
    const float row_height = (body.h - header_height) / 2.f;
    const SDL_Rect player_one =
        MakeRect(body.x, RectBottom(header), body.w, row_height);
    const SDL_Rect player_two =
        MakeRect(body.x, RectBottom(player_one), body.w, row_height);
    draw_list->AddRectFilled(RectMin(header), RectMax(header),
                             kPanelMutedColor);
    draw_list->AddRectFilled(RectMin(player_two), RectMax(player_two),
                             IM_COL32(255, 255, 255, 5));

    constexpr std::array<float, 6> kColumnFractions = {.16f, .30f, .11f,
                                                       .11f, .15f, .17f};
    const std::array<std::string, 6> labels = {
        text.input,    text.direction, text.button_a,
        text.button_b, text.select,    text.start,
    };
    float x = header.x;
    for (size_t i = 0; i < labels.size(); ++i) {
      const float right = i + 1 == labels.size()
                              ? RectRight(header)
                              : x + header.w * kColumnFractions[i];
      SDL_Rect label_rect = MakeRect(x, header.y, right - x, header.h);
      DrawTextInRect(labels[i], label_rect,
                     GetSecondaryFontSize(layout.body_font), kMutedTextColor,
                     4.f * layout.scale, true);
      if (i > 0) {
        draw_list->AddLine(ImVec2(x, body.y), ImVec2(x, RectBottom(body)),
                           kBorderColor);
      }
      x = right;
    }
    draw_list->AddLine(ImVec2(body.x, RectBottom(header)),
                       ImVec2(RectRight(body), RectBottom(header)),
                       kBorderColor);
    draw_list->AddLine(ImVec2(body.x, RectBottom(player_one)),
                       ImVec2(RectRight(body), RectBottom(player_one)),
                       kBorderColor);

    draw_wide_row(player_one, text.player_one, player_one_directions, "J", "K",
                  "L", "Enter");
    draw_wide_row(player_two, text.player_two, player_two_directions, "Delete",
                  "End", "PageDown", "Home");
    return;
  }

  auto draw_narrow_row = [&layout, &text](
                             const SDL_Rect& row, const std::string& player,
                             const std::array<std::string, 4>& directions,
                             const char* button_a, const char* button_b,
                             const char* select, const char* start) {
    const float player_height = std::min(28.f * layout.scale, row.h * .26f);
    SDL_Rect player_rect = MakeRect(row.x, row.y, row.w, player_height);
    DrawTextInRect(player, player_rect, layout.body_font, kTextColor,
                   4.f * layout.scale);
    SDL_Rect mappings =
        MakeRect(row.x, RectBottom(player_rect), row.w, row.h - player_rect.h);
    constexpr std::array<float, 5> kColumnFractions = {.34f, .165f, .165f,
                                                       .165f, .165f};
    const std::array<std::string, 5> labels = {
        text.direction, text.button_a, text.button_b, text.select, text.start};
    float x = mappings.x;
    for (size_t i = 0; i < labels.size(); ++i) {
      const float right = i + 1 == labels.size()
                              ? RectRight(mappings)
                              : x + mappings.w * kColumnFractions[i];
      SDL_Rect cell = MakeRect(x, mappings.y, right - x, mappings.h);
      const float label_height = std::min(18.f * layout.scale, cell.h * .35f);
      DrawTextInRect(labels[i], MakeRect(cell.x, cell.y, cell.w, label_height),
                     GetSecondaryFontSize(layout.body_font), kMutedTextColor,
                     2.f, true);
      SDL_Rect keys = MakeRect(cell.x, cell.y + label_height, cell.w,
                               cell.h - label_height);
      if (i == 0) {
        DrawKeyGroup(keys, GetSecondaryFontSize(layout.body_font), layout.scale,
                     {directions[0].c_str(), directions[1].c_str(),
                      directions[2].c_str(), directions[3].c_str()});
      } else {
        const char* key =
            i == 1 ? button_a : (i == 2 ? button_b : (i == 3 ? select : start));
        DrawKeyGroup(keys, GetSecondaryFontSize(layout.body_font), layout.scale,
                     {key}, i == 1);
      }
      x = right;
    }
  };

  const float row_height = body.h / 2.f;
  const SDL_Rect player_one = MakeRect(body.x, body.y, body.w, row_height);
  const SDL_Rect player_two =
      MakeRect(body.x, RectBottom(player_one), body.w, row_height);
  draw_list->AddLine(ImVec2(body.x, RectBottom(player_one)),
                     ImVec2(RectRight(body), RectBottom(player_one)),
                     kBorderColor);
  draw_narrow_row(player_one, text.player_one, player_one_directions, "J", "K",
                  "L", "Enter");
  draw_narrow_row(player_two, text.player_two, player_two_directions, "Delete",
                  "End", "PageDown", "Home");
}

void AboutWidget::DrawGamepad(const FrameText& text,
                              const FrameLayout& layout) {
  const SDL_Rect& body = layout.controls_body;
  if (!IsValidRect(body))
    return;
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddLine(RectMin(body), ImVec2(RectRight(body), body.y),
                     kBorderColor);

  SDL_Rect illustration;
  SDL_Rect mapping;
  if (body.w >= 520.f * layout.scale && body.w > body.h * 1.05f) {
    illustration = MakeRect(body.x, body.y, body.w * .36f, body.h);
    mapping = MakeRect(RectRight(illustration), body.y, body.w - illustration.w,
                       body.h);
    draw_list->AddLine(ImVec2(RectRight(illustration), body.y),
                       ImVec2(RectRight(illustration), RectBottom(body)),
                       kBorderColor);
  } else {
    illustration = MakeRect(body.x, body.y, body.w, body.h * .34f);
    mapping = MakeRect(body.x, RectBottom(illustration), body.w,
                       body.h - illustration.h);
    draw_list->AddLine(ImVec2(body.x, RectBottom(illustration)),
                       ImVec2(RectRight(body), RectBottom(illustration)),
                       kBorderColor);
  }
  DrawControllerIllustration(illustration, layout.scale);
  SDL_Rect controller_label =
      MakeRect(illustration.x, RectBottom(illustration) - 26.f * layout.scale,
               illustration.w, 22.f * layout.scale);
  DrawTextInRect(text.xbox, controller_label,
                 GetSecondaryFontSize(layout.body_font), kMutedTextColor, 0.f,
                 true, false, FontType::kSystemDefault);

  const std::array<std::string, 6> labels = {
      text.direction, text.button_a, text.button_b,
      text.select,    text.start,    text.menu,
  };
  const std::array<std::string, 6> values = {
      text.xbox_direction, "A", "X", "Y", text.xbox_menu, "LB + RB",
  };
  const float row_height = mapping.h / labels.size();
  for (size_t i = 0; i < labels.size(); ++i) {
    SDL_Rect row =
        MakeRect(mapping.x, mapping.y + row_height * i, mapping.w, row_height);
    SDL_Rect label = MakeRect(row.x, row.y, row.w * .38f, row.h);
    SDL_Rect value = MakeRect(RectRight(label), row.y, row.w - label.w, row.h);
    if (i % 2 == 1)
      draw_list->AddRectFilled(RectMin(row), RectMax(row),
                               IM_COL32(255, 255, 255, 5));
    if (i > 0) {
      draw_list->AddLine(ImVec2(row.x, row.y), ImVec2(RectRight(row), row.y),
                         kBorderColor);
    }
    DrawTextInRect(labels[i], label, GetSecondaryFontSize(layout.body_font),
                   kMutedTextColor, 8.f * layout.scale);
    DrawTextInRect(values[i], value, GetSecondaryFontSize(layout.body_font),
                   kTextColor, 8.f * layout.scale, false, false,
                   FontType::kSystemDefault);
  }
}

void AboutWidget::DrawGameSelection(const FrameText& text,
                                    const FrameLayout& layout) {
  DrawPanel(layout.game_selection_panel);
  DrawSectionTitle(text.game_selection, layout.game_selection_title,
                   layout.body_font, layout.scale);
  if (!IsValidRect(layout.game_selection_body))
    return;

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->PushClipRect(RectMin(layout.game_selection_body),
                          RectMax(layout.game_selection_body), true);
  const PreferredFontSize font_size = GetSecondaryFontSize(layout.body_font);
  const float line_height =
      MeasureText("Ag", font_size, FontType::kSystemDefault).y * 1.35f;
  const float content_right = RectRight(layout.game_selection_body);
  float y = layout.game_selection_body.y;
  const std::string& before_badge = text.game_selection_description[0];
  const std::string& after_badge = text.game_selection_description[1];
  ImVec2 before_size = MeasureText(before_badge, font_size);
  ImVec2 after_size = MeasureText(after_badge, font_size);
  const float badge_size = std::min(
      line_height * .85f, static_cast<float>(layout.game_selection_body.h));

  DrawTextAt(before_badge, ImVec2(layout.game_selection_body.x, y), font_size,
             kMutedTextColor);
  SDL_Rect badge = MakeRect(
      layout.game_selection_body.x + before_size.x + 3.f * layout.scale,
      y + (line_height - badge_size) / 2.f, badge_size, badge_size);
  if (RectRight(badge) > content_right) {
    badge.x = layout.game_selection_body.x;
    y += line_height;
    badge.y = static_cast<int>(y + (line_height - badge_size) / 2.f);
  }
  SDL_Texture* badge_texture =
      GetImage(window()->renderer(), image_resources::ImageID::kItemBadge);
  draw_list->AddImage(reinterpret_cast<ImTextureID>(badge_texture),
                      RectMin(badge), RectMax(badge));

  const float after_x = RectRight(badge) + 3.f * layout.scale;
  const float after_width = content_right - after_x;
  if (after_size.x <= after_width) {
    DrawTextAt(after_badge, ImVec2(after_x, y), font_size, kMutedTextColor);
    y += line_height;
  } else {
    y += line_height;
    std::string wrapped_after = after_badge;
    while (!wrapped_after.empty() &&
           (wrapped_after.front() == ',' || wrapped_after.front() == ' ')) {
      wrapped_after.erase(wrapped_after.begin());
    }
    const float after_height = MeasureWrappedTextHeight(
        wrapped_after, font_size, layout.game_selection_body.w);
    SDL_Rect after_rect = MakeRect(layout.game_selection_body.x, y,
                                   layout.game_selection_body.w, after_height);
    DrawWrappedTextAtTop(wrapped_after, after_rect, font_size, kMutedTextColor);
    y += after_height;
  }

  y += 5.f * layout.scale;
  SDL_Rect second_line =
      MakeRect(layout.game_selection_body.x, y, layout.game_selection_body.w,
               RectBottom(layout.game_selection_body) - y);
  DrawWrappedTextAtTop(text.game_selection_description[2], second_line,
                       font_size, kMutedTextColor);
  draw_list->PopClipRect();
}

void AboutWidget::DrawAbout(const FrameText& text, const FrameLayout& layout) {
  DrawPanel(layout.about_panel);
  DrawSectionTitle(text.about, layout.about_title, layout.body_font,
                   layout.scale);
  DrawWrappedTextAtTop(text.author, layout.about_body,
                       GetSecondaryFontSize(layout.body_font), kMutedTextColor);

  if (IsValidRect(layout.repository)) {
    const bool hovered = hovered_target_ == HitTarget::kRepository;
    ImGui::GetWindowDrawList()->AddRectFilled(
        RectMin(layout.repository), RectMax(layout.repository),
        hovered ? kBrandSoftColor : kPanelMutedColor, 6.f);
    ImGui::GetWindowDrawList()->AddRect(
        RectMin(layout.repository), RectMax(layout.repository),
        hovered ? kBrandColor : kStrongBorderColor, 6.f);
    DrawTextInRect(text.repository_label, layout.repository,
                   GetSecondaryFontSize(layout.body_font),
                   hovered ? kBrandTextColor : kTextColor, 8.f * layout.scale,
                   true, false, FontType::kSystemDefault);
  }

  if (IsValidRect(layout.metadata)) {
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(layout.metadata.x, layout.metadata.y),
        ImVec2(RectRight(layout.metadata), layout.metadata.y), kBorderColor);
    SDL_Rect license = MakeRect(layout.metadata.x, layout.metadata.y,
                                layout.metadata.w / 2.f, layout.metadata.h);
    SDL_Rect build = MakeRect(RectRight(license), layout.metadata.y,
                              layout.metadata.w - license.w, layout.metadata.h);
    DrawTextInRect("GPL-3.0", license, GetSecondaryFontSize(layout.body_font),
                   kFaintTextColor, 0.f, false, false,
                   FontType::kSystemDefault);
    DrawTextInRect("BUILD 2.0.0", build, GetSecondaryFontSize(layout.body_font),
                   kFaintTextColor, 0.f, false, false,
                   FontType::kSystemDefault);
  }
}

bool AboutWidget::HandleInputEvent(SDL_KeyboardEvent* keyboard,
                                   SDL_ControllerButtonEvent* controller) {
  if (keyboard) {
    auto matches = [this, keyboard](kiwi::nes::ControllerButton button,
                                    SDL_KeyCode key) {
      return keyboard->keysym.sym == key ||
             IsJoystickButtonMatch(runtime_data_, button, keyboard->keysym);
    };
#if KIWI_MOBILE
    if (matches(kiwi::nes::ControllerButton::kLeft, SDLK_LEFT)) {
      MoveMobileSection(-1);
      return true;
    }
    if (matches(kiwi::nes::ControllerButton::kRight, SDLK_RIGHT)) {
      MoveMobileSection(1);
      return true;
    }
#else
    if (matches(kiwi::nes::ControllerButton::kLeft, SDLK_LEFT)) {
      SelectInputPage(InputPage::kKeyboard);
      return true;
    }
    if (matches(kiwi::nes::ControllerButton::kRight, SDLK_RIGHT)) {
      SelectInputPage(InputPage::kGamepad);
      return true;
    }
#endif
    if (matches(kiwi::nes::ControllerButton::kB, SDLK_ESCAPE)) {
      PlayEffect(audio_resources::AudioID::kBack);
      Close();
      return true;
    }
    return false;
  }

  if (controller) {
    switch (controller->button) {
      case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
#if KIWI_MOBILE
        MoveMobileSection(-1);
#else
        SelectInputPage(InputPage::kKeyboard);
#endif
        return true;
      case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
#if KIWI_MOBILE
        MoveMobileSection(1);
#else
        SelectInputPage(InputPage::kGamepad);
#endif
        return true;
      case SDL_CONTROLLER_BUTTON_B:
      case SDL_CONTROLLER_BUTTON_X:
        PlayEffect(audio_resources::AudioID::kBack);
        Close();
        return true;
      default:
        return false;
    }
  }

  if (IsJoystickAxisMotionMatch(kiwi::nes::ControllerButton::kLeft)) {
#if KIWI_MOBILE
    MoveMobileSection(-1);
#else
    SelectInputPage(InputPage::kKeyboard);
#endif
    return true;
  }
  if (IsJoystickAxisMotionMatch(kiwi::nes::ControllerButton::kRight)) {
#if KIWI_MOBILE
    MoveMobileSection(1);
#else
    SelectInputPage(InputPage::kGamepad);
#endif
    return true;
  }
  return false;
}

AboutWidget::HitTarget AboutWidget::HitTest(const FrameLayout& layout,
                                            float x,
                                            float y) const {
  if (PointInRect(layout.back_button, x, y))
    return HitTarget::kBack;
#if KIWI_MOBILE
  if (PointInRect(layout.mobile_section_tabs[0], x, y))
    return HitTarget::kControlsSection;
  if (PointInRect(layout.mobile_section_tabs[1], x, y))
    return HitTarget::kGameSelectionSection;
  if (PointInRect(layout.mobile_section_tabs[2], x, y))
    return HitTarget::kAboutSection;
#endif
  if (PointInRect(layout.keyboard_tab, x, y))
    return HitTarget::kKeyboardTab;
  if (PointInRect(layout.gamepad_tab, x, y))
    return HitTarget::kGamepadTab;
  if (PointInRect(layout.repository, x, y))
    return HitTarget::kRepository;
  return HitTarget::kNone;
}

void AboutWidget::ActivateHitTarget(HitTarget target) {
  switch (target) {
    case HitTarget::kKeyboardTab:
      SelectInputPage(InputPage::kKeyboard);
      break;
    case HitTarget::kGamepadTab:
      SelectInputPage(InputPage::kGamepad);
      break;
    case HitTarget::kRepository:
      OpenRepository();
      break;
    case HitTarget::kBack:
      PlayEffect(audio_resources::AudioID::kBack);
      Close();
      break;
#if KIWI_MOBILE
    case HitTarget::kControlsSection:
      SelectMobileSection(MobileSection::kControls);
      break;
    case HitTarget::kGameSelectionSection:
      SelectMobileSection(MobileSection::kGameSelection);
      break;
    case HitTarget::kAboutSection:
      SelectMobileSection(MobileSection::kAbout);
      break;
#endif
    case HitTarget::kNone:
      break;
  }
}

void AboutWidget::SelectInputPage(InputPage page) {
  if (input_page_ == page)
    return;
  input_page_ = page;
  PlayEffect(audio_resources::AudioID::kSelect);
}

#if KIWI_MOBILE
void AboutWidget::SelectMobileSection(MobileSection section) {
  if (mobile_section_ == section)
    return;
  mobile_section_ = section;
  hovered_target_ = HitTarget::kNone;
  pressed_target_ = HitTarget::kNone;
  PlayEffect(audio_resources::AudioID::kSelect);
}

void AboutWidget::MoveMobileSection(int delta) {
  constexpr int kSectionCount = 3;
  const int current = static_cast<int>(mobile_section_);
  const int next = (current + delta + kSectionCount) % kSectionCount;
  SelectMobileSection(static_cast<MobileSection>(next));
}
#endif

void AboutWidget::OpenRepository() {
  if (SDL_OpenURL(kRepositoryUrl) == 0) {
    PlayEffect(audio_resources::AudioID::kSelect);
    return;
  }
  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to open %s: %s",
              kRepositoryUrl, SDL_GetError());
}
