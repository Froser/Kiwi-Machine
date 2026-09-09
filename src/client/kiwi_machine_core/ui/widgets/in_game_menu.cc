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

#include "ui/widgets/in_game_menu.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <utility>
#include <vector>

#include "build/kiwi_defines.h"
#include "ui/main_window.h"
#include "ui/styles.h"
#include "ui/widgets/canvas.h"
#include "utility/audio_effects.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"

namespace {

constexpr ImU32 kBackdropColor = IM_COL32(3, 7, 3, 184);
constexpr ImU32 kPanelColor = IM_COL32(22, 25, 22, 247);
constexpr ImU32 kPanelMutedColor = IM_COL32(34, 39, 34, 255);
constexpr ImU32 kTextColor = IM_COL32(245, 247, 245, 255);
constexpr ImU32 kMutedTextColor = IM_COL32(169, 176, 169, 255);
constexpr ImU32 kBorderColor = IM_COL32(245, 247, 245, 36);
constexpr ImU32 kStrongBorderColor = IM_COL32(245, 247, 245, 58);
constexpr ImU32 kBrandColor = IM_COL32(101, 216, 75, 255);
constexpr ImU32 kBrandSoftColor = IM_COL32(23, 61, 19, 255);
constexpr ImU32 kBrandOnColor = IM_COL32(16, 40, 12, 255);
constexpr ImU32 kDangerColor = IM_COL32(255, 119, 110, 255);
constexpr float kCornerRadius = 8.f;
constexpr float kPanelCornerRadius = 8.f;
constexpr float kTouchMoveThreshold = 12.f;
constexpr float kLargeMenuFontScale = .8f;
constexpr Sint16 kControllerDeadZone = SDL_JOYSTICK_AXIS_MAX / 3;
constexpr int kControllerRepeatDelayMs = 180;
constexpr float kPi = 3.14159265358979323846f;

constexpr std::array<int, 7> kMenuStringIds = {
    string_resources::IDR_IN_GAME_MENU_CONTINUE,
    string_resources::IDR_IN_GAME_MENU_LOAD_AUTO_SAVE,
    string_resources::IDR_IN_GAME_MENU_LOAD_STATE,
    string_resources::IDR_IN_GAME_MENU_SAVE_STATE,
    string_resources::IDR_IN_GAME_MENU_OPTIONS,
    string_resources::IDR_IN_GAME_MENU_RESET_GAME,
    string_resources::IDR_IN_GAME_MENU_BACK_TO_MAIN,
};

constexpr std::array<int, 7> kSettingsStringIds = {
    string_resources::IDR_IN_GAME_MENU_VOLUME,
#if KIWI_MOBILE
    string_resources::IDR_IN_GAME_MENU_SCALING_MODE,
#else
    string_resources::IDR_IN_GAME_MENU_WINDOW_MODE,
#endif
    string_resources::IDR_IN_GAME_MENU_P1,
    string_resources::IDR_IN_GAME_MENU_SWAP_AB_P1,
    string_resources::IDR_IN_GAME_MENU_P2,
    string_resources::IDR_IN_GAME_MENU_SWAP_AB_P2,
    string_resources::IDR_IN_GAME_MENU_LANGUAGE,
};

template <typename T>
constexpr size_t ToIndex(T value) {
  return static_cast<size_t>(value);
}

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

bool PointInClippedRect(const SDL_Rect& rect,
                        const SDL_Rect& clip,
                        float x,
                        float y) {
  return PointInRect(rect, x, y) && PointInRect(clip, x, y);
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

float GetMenuFontScale(PreferredFontSize size) {
  return size == PreferredFontSize::k1x ? 1.f : kLargeMenuFontScale;
}

float GetMenuHorizontalPadding(float layout_padding) {
#if KIWI_MOBILE
  return std::max(12.f, layout_padding * .5f);
#else
  return layout_padding;
#endif
}

ImVec2 MeasureText(const std::string& text,
                   PreferredFontSize font_size,
                   float wrap_width,
                   FontType default_font = FontType::kDefault,
                   float font_scale = 1.f) {
  ScopedFont font = GetPreferredFont(font_size, text.c_str(), default_font);
  return font.GetFont()->CalcTextSizeA(font.GetFontSize() * font_scale, FLT_MAX,
                                       wrap_width, text.c_str());
}

float MeasureTextHeight(const std::string& text,
                        PreferredFontSize font_size,
                        float wrap_width,
                        FontType default_font = FontType::kDefault,
                        float font_scale = 1.f) {
  return MeasureText(text, font_size, wrap_width, default_font, font_scale).y;
}

void DrawTextInRect(const std::string& text,
                    const SDL_Rect& rect,
                    PreferredFontSize font_size,
                    ImU32 color,
                    float horizontal_padding,
                    bool center,
                    bool wrap,
                    FontType default_font = FontType::kDefault,
                    float font_scale = 1.f) {
  if (!IsValidRect(rect) || text.empty())
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
  float y = rect.y + std::max(0.f, (rect.h - text_size.y) / 2.f);
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

void DrawCloseIcon(const SDL_Rect& rect, ImU32 color) {
  const float inset = std::min(rect.w, rect.h) * .32f;
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddLine(ImVec2(rect.x + inset, rect.y + inset),
                     ImVec2(RectRight(rect) - inset, RectBottom(rect) - inset),
                     color, 2.f);
  draw_list->AddLine(ImVec2(RectRight(rect) - inset, rect.y + inset),
                     ImVec2(rect.x + inset, RectBottom(rect) - inset), color,
                     2.f);
}

void DrawButtonBackground(const SDL_Rect& rect,
                          bool selected,
                          bool hovered,
                          bool primary,
                          bool danger,
                          bool enabled = true) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImU32 fill = IM_COL32(0, 0, 0, 0);
  ImU32 border = kBorderColor;
  if (!enabled) {
    border = IM_COL32(245, 247, 245, 18);
  } else if (primary) {
    fill = kBrandColor;
    border = selected ? kTextColor : fill;
  } else if (danger && (selected || hovered)) {
    fill = IM_COL32(92, 31, 28, 180);
    border = kDangerColor;
  } else if (selected || hovered) {
    fill = kBrandSoftColor;
    border = selected ? kBrandColor : kStrongBorderColor;
  }

  draw_list->AddRectFilled(RectMin(rect), RectMax(rect), fill, kCornerRadius);
  draw_list->AddRect(RectMin(rect), RectMax(rect), border, kCornerRadius, 0,
                     selected ? 2.f : 1.f);
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

}  // namespace

InGameMenu::InGameMenu(MainWindow* main_window,
                       NESRuntimeID runtime_id,
                       MenuItemCallback menu_callback,
                       SettingsItemCallback settings_callback)
    : Widget(main_window),
      main_window_(main_window),
      runtime_data_(NESRuntime::GetInstance()->GetDataById(runtime_id)),
      menu_callback_(std::move(menu_callback)),
      settings_callback_(std::move(settings_callback)) {
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("InGameMenu");
  SDL_assert(runtime_data_);

  menu_item_visible_.fill(true);
  SetFirstSelection();
}

InGameMenu::~InGameMenu() {
  lifetime_token_.reset();
  if (snapshot_)
    SDL_DestroyTexture(snapshot_);
}

void InGameMenu::Close() {
  CancelStatePreviewRequest();
  ++auto_save_count_request_id_;
  touch_active_ = false;
  mouse_pressed_ = false;
  has_layout_ = false;
  set_visible(false);
}

void InGameMenu::Show() {
  page_ = Page::kMainMenu;
  focus_.area = FocusArea::kMenu;
  focus_.settings_item = SettingsItem::kVolume;
  focus_.confirm_action = false;
  menu_scroll_offset_ = 0.f;
  settings_scroll_offset_ = 0.f;
  has_layout_ = false;
  SetFirstSelection();
  if (focus_.menu_item == MenuItem::kLoadAutoSave)
    RequestAutoSavedStateCount();
  if (IsStateMenuItem(focus_.menu_item))
    RefreshStatePreview();
  set_visible(true);
}

void InGameMenu::SetMenuItemVisible(MenuItem item, bool visible) {
  menu_item_visible_[ToIndex(item)] = visible;
  if (!visible && focus_.menu_item == item) {
    SetFirstSelection();
    if (focus_.menu_item == MenuItem::kLoadAutoSave)
      RequestAutoSavedStateCount();
    if (IsStateMenuItem(focus_.menu_item))
      RefreshStatePreview();
  }
}

void InGameMenu::Paint() {
  FrameText text = BuildFrameText();
  FrameLayout layout = CalculateFrameLayout(text);
  last_layout_ = layout;
  has_layout_ = true;
  DrawFrame(text, layout);
}

bool InGameMenu::OnKeyPressed(SDL_KeyboardEvent* event) {
  HandleInputEvent(event, nullptr);
  return true;
}

bool InGameMenu::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  HandleInputEvent(nullptr, event);
  return true;
}

bool InGameMenu::OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) {
  HandleControllerAxis(event);
  return true;
}

bool InGameMenu::OnMouseMove(SDL_MouseMotionEvent* event) {
  if (!has_layout_)
    return true;

  HitTarget target = HitTest(last_layout_, event->x, event->y);
  UpdatePointerFocus(target, InputModality::kMouse);
  if (mouse_pressed_ && pressed_target_.type == HitTargetType::kSettingValue &&
      pressed_target_.index == static_cast<int>(SettingsItem::kVolume)) {
    SetVolumeFromPoint(
        event->x, last_layout_.setting_values[ToIndex(SettingsItem::kVolume)]);
  }
  return true;
}

bool InGameMenu::OnMouseWheel(SDL_MouseWheelEvent* event) {
  if (!has_layout_)
    return true;

  if (last_layout_.visible_page == Page::kSettings &&
      last_layout_.settings_content_height > last_layout_.detail.h) {
    settings_scroll_offset_ -= event->preciseY * last_layout_.row_height;
    ClampSettingsScroll(last_layout_);
    return true;
  }

  if (last_layout_.mode != LayoutMode::kSinglePane ||
      page_ != Page::kMainMenu ||
      last_layout_.menu_content_height <= last_layout_.navigation.h) {
    return true;
  }

  menu_scroll_offset_ -= event->preciseY * last_layout_.row_height;
  ClampMenuScroll(last_layout_);
  return true;
}

bool InGameMenu::OnMousePressed(SDL_MouseButtonEvent* event) {
  if (!has_layout_ || event->button != SDL_BUTTON_LEFT)
    return true;

  pressed_target_ = HitTest(last_layout_, event->x, event->y);
  mouse_pressed_ = pressed_target_.type != HitTargetType::kNone;
  UpdatePointerFocus(pressed_target_, InputModality::kMouse);
  if (pressed_target_.type == HitTargetType::kSettingValue &&
      pressed_target_.index == static_cast<int>(SettingsItem::kVolume)) {
    SetVolumeFromPoint(
        event->x, last_layout_.setting_values[ToIndex(SettingsItem::kVolume)]);
  }
  return true;
}

bool InGameMenu::OnMouseReleased(SDL_MouseButtonEvent* event) {
  if (event->button == SDL_BUTTON_RIGHT) {
    return HandleNavigationAction(NavigationAction::kBack,
                                  InputModality::kMouse);
  }
  if (event->button != SDL_BUTTON_LEFT || !mouse_pressed_)
    return true;

  HitTarget released_target = HitTest(last_layout_, event->x, event->y);
  const bool should_activate = released_target == pressed_target_;
  mouse_pressed_ = false;
  pressed_target_ = {};
  if (should_activate)
    ActivateHitTarget(released_target, event->x);
  return true;
}

bool InGameMenu::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  SDL_Rect window_bounds = window()->GetClientBounds();
  ImVec2 point(event->x * window_bounds.w, event->y * window_bounds.h);
  touch_active_ = true;
  active_touch_id_ = event->fingerId;
  touch_down_position_ = point;
  last_touch_position_ = point;
  touch_moved_ = false;
  pressed_target_ = HitTest(last_layout_, point.x, point.y);
  UpdatePointerFocus(pressed_target_, InputModality::kTouch);
  if (pressed_target_.type == HitTargetType::kSettingValue &&
      pressed_target_.index == static_cast<int>(SettingsItem::kVolume)) {
    SetVolumeFromPoint(
        point.x, last_layout_.setting_values[ToIndex(SettingsItem::kVolume)]);
  }
  return true;
}

bool InGameMenu::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  if (!touch_active_ || event->fingerId != active_touch_id_)
    return true;

  SDL_Rect window_bounds = window()->GetClientBounds();
  ImVec2 point(event->x * window_bounds.w, event->y * window_bounds.h);
  const float delta_x = point.x - touch_down_position_.x;
  const float delta_y = point.y - touch_down_position_.y;
  if (std::sqrt(delta_x * delta_x + delta_y * delta_y) > kTouchMoveThreshold) {
    touch_moved_ = true;
  }

  if (pressed_target_.type == HitTargetType::kSettingValue &&
      pressed_target_.index == static_cast<int>(SettingsItem::kVolume)) {
    SetVolumeFromPoint(
        point.x, last_layout_.setting_values[ToIndex(SettingsItem::kVolume)]);
  } else if (last_layout_.visible_page == Page::kSettings &&
             last_layout_.settings_content_height > last_layout_.detail.h) {
    settings_scroll_offset_ -= point.y - last_touch_position_.y;
    ClampSettingsScroll(last_layout_);
  } else if (last_layout_.mode == LayoutMode::kSinglePane &&
             page_ == Page::kMainMenu &&
             last_layout_.menu_content_height > last_layout_.navigation.h) {
    menu_scroll_offset_ -= point.y - last_touch_position_.y;
    ClampMenuScroll(last_layout_);
  }

  last_touch_position_ = point;
  return true;
}

bool InGameMenu::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  if (!touch_active_ || event->fingerId != active_touch_id_)
    return true;

  SDL_Rect window_bounds = window()->GetClientBounds();
  ImVec2 point(event->x * window_bounds.w, event->y * window_bounds.h);
  HitTarget released_target = HitTest(last_layout_, point.x, point.y);
  if (!touch_moved_ && released_target == pressed_target_)
    ActivateHitTarget(released_target, point.x);

  touch_active_ = false;
  touch_moved_ = false;
  pressed_target_ = {};
  return true;
}

void InGameMenu::OnWindowPreRender() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
}

void InGameMenu::OnWindowPostRender() {
  ImGui::PopStyleVar(2);
}

InGameMenu::FrameText InGameMenu::BuildFrameText() const {
  FrameText text;
  for (size_t i = 0; i < text.menu_items.size(); ++i)
    text.menu_items[i] = GetLocalizedString(kMenuStringIds[i]);
  for (size_t i = 0; i < text.setting_labels.size(); ++i) {
    text.setting_labels[i] = GetLocalizedString(kSettingsStringIds[i]);
    text.setting_values[i] = GetSettingValue(static_cast<SettingsItem>(i));
  }

  text.header =
      IsStandaloneSettingsMenu()
          ? text.menu_items[ToIndex(MenuItem::kOptions)]
          : GetLocalizedString(string_resources::IDR_IN_GAME_MENU_PAUSED);
  text.subtitle = main_window_->title();

  text.state_title = text.menu_items[ToIndex(focus_.menu_item)];
  if (focus_.menu_item == MenuItem::kLoadAutoSave) {
    if (current_auto_states_count_ > 0) {
      text.state_position = std::to_string(which_autosave_state_slot_ + 1) +
                            " / " + std::to_string(current_auto_states_count_);
    } else {
      text.state_position =
          GetLocalizedString(string_resources::IDR_IN_GAME_MENU_NO_STATE);
    }
  } else {
    text.state_position =
        GetLocalizedString(string_resources::IDR_IN_GAME_MENU_SLOT) +
        std::to_string(which_state_ + 1) + " / " +
        std::to_string(NESRuntime::Data::MaxSaveStates);
  }

  if (focus_.menu_item == MenuItem::kLoadAutoSave && state_preview_.timestamp) {
    time_t timestamp = state_preview_.timestamp;
    if (std::tm* local_time = std::localtime(&timestamp)) {
      char time_buffer[32] = {};
      if (std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M",
                        local_time)) {
        text.state_metadata = time_buffer;
      }
    }
  }

  text.state_action = GetLocalizedString(
      focus_.menu_item == MenuItem::kSaveState
          ? string_resources::IDR_IN_GAME_MENU_SAVE_TO_SLOT
          : string_resources::IDR_IN_GAME_MENU_LOAD_THIS_STATE);
  text.confirmation_title =
      GetLocalizedString(string_resources::IDR_IN_GAME_MENU_CONFIRM_ACTION);
  text.confirmation_message = GetLocalizedString(
      confirmation_item_ == MenuItem::kResetGame
          ? string_resources::IDR_IN_GAME_MENU_CONFIRM_RESET
          : string_resources::IDR_IN_GAME_MENU_CONFIRM_BACK_TO_MAIN);
  text.cancel = GetLocalizedString(string_resources::IDR_IN_GAME_MENU_CANCEL);
  text.confirm = GetLocalizedString(string_resources::IDR_COMMON_CONFIRM);
  return text;
}

InGameMenu::FrameLayout InGameMenu::CalculateFrameLayout(
    const FrameText& text) {
  FrameLayout layout;
  SDL_Rect safe_area_insets = main_window_->GetSafeAreaInsets();
  ImVec2 window_pos = ImGui::GetWindowPos();
  ImVec2 window_size = ImGui::GetWindowSize();
  layout.safe_area = MakeRect(
      window_pos.x + safe_area_insets.x, window_pos.y + safe_area_insets.y,
      window_size.x - safe_area_insets.x - safe_area_insets.w,
      window_size.y - safe_area_insets.y - safe_area_insets.h);
  layout.font_size =
      styles::in_game_menu::GetPreferredFontSize(main_window_->window_scale());

  float widest_menu_item = 0.f;
  {
    ScopedFont font =
        GetPreferredFont(layout.font_size, text.menu_items[0].c_str());
    layout.font_height = font.GetFontSize();
    const float menu_font_scale = GetMenuFontScale(layout.font_size);
    for (size_t i = 0; i < text.menu_items.size(); ++i) {
      if (menu_item_visible_[i]) {
        widest_menu_item =
            std::max(widest_menu_item,
                     MeasureText(text.menu_items[i], layout.font_size, 0.f,
                                 FontType::kDefault, menu_font_scale)
                         .x);
      }
    }
  }

  const float scale = std::clamp(layout.font_height / 16.f, 1.f, 2.f);
  layout.padding = std::max(12.f, layout.font_height * .75f);
  layout.row_height =
      std::max(44.f, layout.font_height + layout.padding * 1.5f);

  const float candidate_margin = std::min(32.f, std::max(16.f, 24.f * scale));
  SDL_Rect candidate_panel = InsetRect(layout.safe_area, candidate_margin);
#if KIWI_MOBILE
  const float menu_horizontal_padding =
      GetMenuHorizontalPadding(layout.padding);
  const float chevron_width = std::max(36.f, layout.font_height);
  const float navigation_width =
      std::max(190.f * scale, widest_menu_item + chevron_width +
                                  menu_horizontal_padding * 4.f);
#else
  const float navigation_width =
      std::max(190.f * scale, widest_menu_item + layout.padding * 3.f);
#endif
#if KIWI_MOBILE
  const float detail_min_width =
      std::max(240.f * scale, layout.font_height * 10.f);
#else
  const float detail_min_width =
      std::max(300.f * scale, layout.font_height * 15.f);
#endif
  layout.mode = candidate_panel.w >= navigation_width + detail_min_width &&
                        candidate_panel.w > candidate_panel.h * 1.1f
                    ? LayoutMode::kTwoPane
                    : LayoutMode::kSinglePane;

  layout.panel =
      layout.mode == LayoutMode::kTwoPane ? candidate_panel : layout.safe_area;
  const float icon_size = std::max(44.f, layout.font_height + layout.padding);
  const float header_text_width = std::max(
      1.f, layout.panel.w - layout.padding * 3.f - icon_size -
               (page_ == Page::kMainMenu ? 0.f : icon_size + layout.padding));
  const float title_height =
      MeasureTextHeight(text.header, layout.font_size, header_text_width);
  const float subtitle_height =
      MeasureTextHeight(text.subtitle, GetSecondaryFontSize(layout.font_size),
                        header_text_width, FontType::kSystemDefault);
  const float header_height =
      std::max(icon_size + layout.padding,
               title_height + subtitle_height + layout.padding * 1.5f);
  layout.header =
      MakeRect(layout.panel.x, layout.panel.y, layout.panel.w, header_height);
  layout.content = MakeRect(layout.panel.x, RectBottom(layout.header),
                            layout.panel.w, layout.panel.h - layout.header.h);

  layout.close_button =
      MakeRect(RectRight(layout.header) - layout.padding - icon_size,
               layout.header.y + (layout.header.h - icon_size) / 2.f, icon_size,
               icon_size);
  if (page_ != Page::kMainMenu && layout.mode == LayoutMode::kSinglePane) {
    layout.back_button =
        MakeRect(layout.header.x + layout.padding,
                 layout.header.y + (layout.header.h - icon_size) / 2.f,
                 icon_size, icon_size);
  }
  const float title_left = IsValidRect(layout.back_button)
                               ? RectRight(layout.back_button) + layout.padding
                               : layout.header.x + layout.padding;
  const float header_text_y =
      layout.header.y +
      (layout.header.h - title_height - subtitle_height) / 2.f;
  layout.header_title = MakeRect(
      title_left, header_text_y,
      layout.close_button.x - layout.padding - title_left, title_height);
  layout.header_subtitle = MakeRect(title_left, RectBottom(layout.header_title),
                                    layout.header_title.w, subtitle_height);

  if (layout.mode == LayoutMode::kTwoPane) {
#if KIWI_MOBILE
    const float resolved_navigation_width = navigation_width;
#else
    const float resolved_navigation_width =
        std::min(navigation_width, layout.content.w * .42f);
#endif
    layout.navigation = MakeRect(layout.content.x, layout.content.y,
                                 resolved_navigation_width, layout.content.h);
    layout.detail =
        MakeRect(RectRight(layout.navigation), layout.content.y,
                 layout.content.w - layout.navigation.w, layout.content.h);
    layout.visible_page = page_ == Page::kMainMenu ? GetPreviewPage() : page_;
  } else if (page_ == Page::kMainMenu) {
    layout.navigation = layout.content;
    layout.visible_page = Page::kMainMenu;
  } else {
    layout.detail = layout.content;
    layout.visible_page = page_;
  }

  if (IsValidRect(layout.navigation))
    LayoutMenu(text, layout);
  switch (layout.visible_page) {
    case Page::kStateBrowser:
      LayoutStateBrowser(layout);
      break;
    case Page::kSettings:
      LayoutSettings(text, layout);
      break;
    case Page::kConfirmation:
      LayoutConfirmation(layout);
      break;
    case Page::kMainMenu: {
      if (layout.mode == LayoutMode::kTwoPane) {
#if KIWI_MOBILE
        const std::string& action_text =
            text.menu_items[ToIndex(focus_.menu_item)];
        const float text_width =
            MeasureText(action_text, layout.font_size, 0.f).x;
        const float width =
            std::min(std::max(240.f, text_width + layout.padding * 2.f),
                     layout.detail.w - layout.padding * 2.f);
#else
        const float width =
            std::min(240.f, layout.detail.w - 2.f * layout.padding);
#endif
        layout.contextual_action = MakeRect(
            layout.detail.x + (layout.detail.w - width) / 2.f,
            layout.detail.y + (layout.detail.h - layout.row_height) / 2.f,
            width, layout.row_height);
      }
      break;
    }
  }

  return layout;
}

void InGameMenu::LayoutMenu(const FrameText& text, FrameLayout& layout) {
  std::array<float, kMenuItemCount> row_heights = {};
  const float horizontal_padding = GetMenuHorizontalPadding(layout.padding);
#if KIWI_MOBILE
  const float text_width = 0.f;
#else
  const float text_width =
      std::max(1.f, layout.navigation.w - layout.padding * 3.f);
#endif
  const float menu_font_scale = GetMenuFontScale(layout.font_size);
  float top_height = 0.f;
  float bottom_height = 0.f;
  int top_count = 0;
  int bottom_count = 0;
  for (size_t i = 0; i < kMenuItemCount; ++i) {
    if (!menu_item_visible_[i])
      continue;
    row_heights[i] = std::max(
        layout.row_height,
        MeasureTextHeight(text.menu_items[i], layout.font_size, text_width,
                          FontType::kDefault, menu_font_scale) +
            layout.padding);
    if (i <= ToIndex(MenuItem::kOptions)) {
      top_height += row_heights[i];
      ++top_count;
    } else {
      bottom_height += row_heights[i];
      ++bottom_count;
    }
  }

  constexpr float kRowGap = 4.f;
  const float group_gap = layout.padding;
  if (top_count > 1)
    top_height += (top_count - 1) * kRowGap;
  if (bottom_count > 1)
    bottom_height += (bottom_count - 1) * kRowGap;
  const float usable_height =
      std::max(0.f, layout.navigation.h - layout.padding * 2.f);
  const bool groups_fit = top_height + bottom_height +
                              (top_count && bottom_count ? group_gap : 0.f) <=
                          usable_height;

  layout.menu_content_height = top_height + bottom_height +
                               (top_count && bottom_count ? group_gap : 0.f) +
                               layout.padding * 2.f;
  ClampMenuScroll(layout);

  float top_y = layout.navigation.y + layout.padding - menu_scroll_offset_;
  for (size_t i = 0; i <= ToIndex(MenuItem::kOptions); ++i) {
    if (!menu_item_visible_[i])
      continue;
    layout.menu_items[i] = MakeRect(
        layout.navigation.x + horizontal_padding, top_y,
        layout.navigation.w - horizontal_padding * 2.f, row_heights[i]);
    top_y += row_heights[i] + kRowGap;
  }

  float bottom_y =
      groups_fit
          ? RectBottom(layout.navigation) - layout.padding - bottom_height
          : top_y + (top_count && bottom_count ? group_gap : 0.f);
  for (size_t i = ToIndex(MenuItem::kResetGame);
       i <= ToIndex(MenuItem::kToGameSelection); ++i) {
    if (!menu_item_visible_[i])
      continue;
    layout.menu_items[i] = MakeRect(
        layout.navigation.x + horizontal_padding, bottom_y,
        layout.navigation.w - horizontal_padding * 2.f, row_heights[i]);
    bottom_y += row_heights[i] + kRowGap;
  }
}

void InGameMenu::LayoutStateBrowser(FrameLayout& layout) {
  SDL_Rect inner = InsetRect(layout.detail, layout.padding);
  const float gap = layout.padding;
  const float action_height = layout.row_height;
  const float text_height = layout.font_height * 1.5f;
  const bool horizontal = inner.w >= 520.f && inner.w > inner.h * 1.15f;

  if (horizontal) {
    const float preview_width =
        std::min((inner.w - gap) * .56f,
                 inner.h * Canvas::kNESFrameDefaultWidth /
                     static_cast<float>(Canvas::kNESFrameDefaultHeight));
    const float preview_height =
        preview_width * Canvas::kNESFrameDefaultHeight /
        static_cast<float>(Canvas::kNESFrameDefaultWidth);
    layout.state_preview =
        MakeRect(inner.x, inner.y + (inner.h - preview_height) / 2.f,
                 preview_width, preview_height);

    SDL_Rect controls =
        MakeRect(RectRight(layout.state_preview) + gap, inner.y,
                 inner.w - layout.state_preview.w - gap, inner.h);
    const float controls_height =
        text_height * 2.f + gap * 3.f + action_height * 2.f;
    float y = controls.y + std::max(0.f, (controls.h - controls_height) / 2.f);
    layout.state_title = MakeRect(controls.x, y, controls.w, text_height);
    y += text_height;
    layout.state_metadata = MakeRect(controls.x, y, controls.w, text_height);
    y += text_height + gap;
    const float button_width = action_height;
    layout.state_previous =
        MakeRect(controls.x, y, button_width, action_height);
    layout.state_next = MakeRect(RectRight(controls) - button_width, y,
                                 button_width, action_height);
    layout.state_position =
        MakeRect(RectRight(layout.state_previous), y,
                 controls.w - button_width * 2.f, action_height);
    y += action_height + gap;
    layout.state_action = MakeRect(controls.x, y, controls.w, action_height);
  } else {
    const float preview_width = inner.w;
    const float natural_preview_height =
        preview_width * Canvas::kNESFrameDefaultHeight /
        static_cast<float>(Canvas::kNESFrameDefaultWidth);
    const float controls_height =
        text_height * 2.f + gap * 3.f + action_height * 2.f;
    const float preview_height = std::min(
        natural_preview_height, std::max(0.f, inner.h - controls_height));
    const float fitted_preview_width =
        preview_height * Canvas::kNESFrameDefaultWidth /
        static_cast<float>(Canvas::kNESFrameDefaultHeight);
    layout.state_preview =
        MakeRect(inner.x + (inner.w - fitted_preview_width) / 2.f, inner.y,
                 fitted_preview_width, preview_height);

    float y = RectBottom(layout.state_preview) + gap;
    layout.state_title = MakeRect(inner.x, y, inner.w, text_height);
    y += text_height;
    layout.state_metadata = MakeRect(inner.x, y, inner.w, text_height);
    y += text_height + gap;
    const float button_width = action_height;
    layout.state_previous = MakeRect(inner.x, y, button_width, action_height);
    layout.state_next = MakeRect(RectRight(inner) - button_width, y,
                                 button_width, action_height);
    layout.state_position =
        MakeRect(RectRight(layout.state_previous), y,
                 inner.w - button_width * 2.f, action_height);
    y += action_height + gap;
    layout.state_action = MakeRect(inner.x, y, inner.w, action_height);
  }
}

void InGameMenu::LayoutSettings(const FrameText& text, FrameLayout& layout) {
  SDL_Rect inner = InsetRect(layout.detail, layout.padding);
  constexpr float kRowGap = 8.f;
  const float control_width =
      std::min(std::max(150.f, inner.w * .54f), inner.w * .66f);
  const float label_width =
      std::max(1.f, inner.w - control_width - layout.padding * 2.f);
  const float value_width =
      std::max(1.f, control_width - layout.row_height * 2.f - layout.padding);
  std::array<float, kSettingsItemCount> row_heights = {};
  float required_height = kRowGap * (kSettingsItemCount - 1);
  for (size_t i = 0; i < kSettingsItemCount; ++i) {
    const float label_height = MeasureTextHeight(text.setting_labels[i],
                                                 layout.font_size, label_width);
    const float value_height = MeasureTextHeight(
        text.setting_values[i], GetSecondaryFontSize(layout.font_size),
        value_width, FontType::kSystemDefault);
    row_heights[i] =
        std::max(layout.row_height,
                 std::max(label_height, value_height) + layout.padding);
    required_height += row_heights[i];
  }

  layout.settings_content_height = required_height + layout.padding * 2.f;
  ClampSettingsScroll(layout);
  float y = inner.y + std::max(0.f, (inner.h - required_height) / 2.f) -
            settings_scroll_offset_;
  for (size_t i = 0; i < kSettingsItemCount; ++i) {
    const float row_height = row_heights[i];
    layout.setting_items[i] = MakeRect(inner.x, y, inner.w, row_height);
    const float button_width = std::min(layout.row_height, control_width / 3.f);
    const float control_x = RectRight(inner) - control_width;
    layout.setting_previous[i] =
        MakeRect(control_x, y, button_width, row_height);
    layout.setting_next[i] =
        MakeRect(RectRight(inner) - button_width, y, button_width, row_height);
    layout.setting_values[i] =
        MakeRect(RectRight(layout.setting_previous[i]), y,
                 control_width - button_width * 2.f, row_height);
    y += row_height + kRowGap;
  }
}

void InGameMenu::LayoutConfirmation(FrameLayout& layout) {
  const float body_width =
      std::min(440.f, layout.detail.w - layout.padding * 2.f);
  const float body_height = std::min(std::max(180.f, layout.font_height * 7.f),
                                     layout.detail.h - layout.padding * 2.f);
  layout.confirmation_body =
      MakeRect(layout.detail.x + (layout.detail.w - body_width) / 2.f,
               layout.detail.y + (layout.detail.h - body_height) / 2.f,
               body_width, body_height);
  const float button_gap = layout.padding;
  const float button_width =
      (body_width - layout.padding * 2.f - button_gap) / 2.f;
  layout.confirmation_cancel = MakeRect(
      layout.confirmation_body.x + layout.padding,
      RectBottom(layout.confirmation_body) - layout.padding - layout.row_height,
      button_width, layout.row_height);
  layout.confirmation_accept =
      MakeRect(RectRight(layout.confirmation_cancel) + button_gap,
               layout.confirmation_cancel.y, button_width, layout.row_height);
}

void InGameMenu::DrawFrame(const FrameText& text, const FrameLayout& layout) {
  DrawBackground(layout);
  DrawHeader(text, layout);
  if (IsValidRect(layout.navigation))
    DrawMenu(text, layout);

  switch (layout.visible_page) {
    case Page::kStateBrowser:
      DrawStateBrowser(text, layout);
      break;
    case Page::kSettings:
      DrawSettings(text, layout);
      break;
    case Page::kConfirmation:
      DrawConfirmation(text, layout);
      break;
    case Page::kMainMenu:
      DrawContextualAction(text, layout);
      break;
  }

  ImGui::SetCursorScreenPos(RectMax(layout.panel));
  ImGui::Dummy(ImVec2(0.f, 0.f));
}

void InGameMenu::DrawBackground(const FrameLayout& layout) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 window_pos = ImGui::GetWindowPos();
  ImVec2 window_size = ImGui::GetWindowSize();
  draw_list->AddRectFilled(
      window_pos,
      ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
      kBackdropColor);
  draw_list->AddRectFilled(
      RectMin(layout.panel), RectMax(layout.panel), kPanelColor,
      layout.mode == LayoutMode::kTwoPane ? kPanelCornerRadius : 0.f);
  draw_list->AddRect(
      RectMin(layout.panel), RectMax(layout.panel), kBorderColor,
      layout.mode == LayoutMode::kTwoPane ? kPanelCornerRadius : 0.f);
  if (layout.mode == LayoutMode::kTwoPane) {
    draw_list->AddLine(
        ImVec2(RectRight(layout.navigation), layout.navigation.y),
        ImVec2(RectRight(layout.navigation), RectBottom(layout.navigation)),
        kBorderColor);
  }
}

void InGameMenu::DrawHeader(const FrameText& text, const FrameLayout& layout) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddLine(
      ImVec2(layout.header.x, RectBottom(layout.header)),
      ImVec2(RectRight(layout.header), RectBottom(layout.header)),
      kBorderColor);

  if (IsValidRect(layout.back_button)) {
    const bool hovered = hovered_target_.type == HitTargetType::kBack;
    DrawButtonBackground(layout.back_button, false, hovered, false, false);
    DrawChevron(layout.back_button, true, kTextColor);
  }
  DrawTextInRect(text.header, layout.header_title, layout.font_size, kTextColor,
                 0.f, false, true);
  DrawTextInRect(text.subtitle, layout.header_subtitle,
                 GetSecondaryFontSize(layout.font_size), kMutedTextColor, 0.f,
                 false, true, FontType::kSystemDefault);

  const bool close_hovered = hovered_target_.type == HitTargetType::kClose;
  DrawButtonBackground(layout.close_button, false, close_hovered, false, false);
  DrawCloseIcon(layout.close_button, kTextColor);
}

void InGameMenu::DrawMenu(const FrameText& text, const FrameLayout& layout) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->PushClipRect(RectMin(layout.navigation),
                          RectMax(layout.navigation), true);
  for (size_t i = 0; i < kMenuItemCount; ++i) {
    if (!menu_item_visible_[i] || !IsValidRect(layout.menu_items[i]))
      continue;

    const MenuItem item = static_cast<MenuItem>(i);
    const bool selected = focus_.menu_item == item;
    const bool hovered = hovered_target_ == HitTarget{HitTargetType::kMenuItem,
                                                      static_cast<int>(i)};
    const bool primary = item == MenuItem::kContinue;
    const bool danger = item == MenuItem::kResetGame;
    DrawButtonBackground(layout.menu_items[i], selected, hovered, primary,
                         danger);

    ImU32 text_color = kTextColor;
    if (primary)
      text_color = kBrandOnColor;
    else if (danger)
      text_color = kDangerColor;
#if KIWI_MOBILE
    const float chevron_width = std::max(36.f, layout.font_height);
#else
    const float chevron_width = layout.row_height;
#endif
    const float horizontal_padding = GetMenuHorizontalPadding(layout.padding);
    SDL_Rect text_rect = MakeRect(
        layout.menu_items[i].x, layout.menu_items[i].y,
        layout.menu_items[i].w - chevron_width, layout.menu_items[i].h);
    DrawTextInRect(text.menu_items[i], text_rect, layout.font_size, text_color,
                   horizontal_padding, false,
#if KIWI_MOBILE
                   false,
#else
                   true,
#endif
                   FontType::kDefault, GetMenuFontScale(layout.font_size));
    SDL_Rect chevron_rect =
        MakeRect(RectRight(layout.menu_items[i]) - chevron_width,
                 layout.menu_items[i].y, chevron_width, layout.menu_items[i].h);
    DrawChevron(chevron_rect, false,
                primary
                    ? kBrandOnColor
                    : (selected || hovered ? kBrandColor : kMutedTextColor));
  }
  if (layout.menu_content_height > layout.navigation.h) {
    const float track_height =
        std::max(1.f, layout.navigation.h - layout.padding * 2.f);
    const float thumb_height = std::max(
        24.f, track_height * layout.navigation.h / layout.menu_content_height);
    const float max_scroll = layout.menu_content_height - layout.navigation.h;
    const float thumb_y =
        layout.navigation.y + layout.padding +
        (track_height - thumb_height) * menu_scroll_offset_ / max_scroll;
    const float thumb_x = RectRight(layout.navigation) - 4.f;
    draw_list->AddRectFilled(ImVec2(thumb_x, thumb_y),
                             ImVec2(thumb_x + 2.f, thumb_y + thumb_height),
                             kMutedTextColor, 1.f);
  }
  draw_list->PopClipRect();
}

void InGameMenu::DrawStateBrowser(const FrameText& text,
                                  const FrameLayout& layout) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(RectMin(layout.state_preview),
                           RectMax(layout.state_preview), kPanelMutedColor,
                           kCornerRadius);

  if (state_preview_.status == StatePreview::Status::kLoading) {
    DrawSpinner(layout.state_preview);
  } else if (state_preview_.status == StatePreview::Status::kReady &&
             state_preview_.has_thumbnail && snapshot_) {
    draw_list->AddImage(reinterpret_cast<ImTextureID>(snapshot_),
                        RectMin(layout.state_preview),
                        RectMax(layout.state_preview));
  } else {
    DrawTextInRect(
        GetLocalizedString(string_resources::IDR_IN_GAME_MENU_NO_STATE),
        layout.state_preview, GetSecondaryFontSize(layout.font_size),
        kMutedTextColor, layout.padding, true, true, FontType::kSystemDefault);
  }
  draw_list->AddRect(RectMin(layout.state_preview),
                     RectMax(layout.state_preview), kStrongBorderColor,
                     kCornerRadius);

  DrawTextInRect(text.state_title, layout.state_title, layout.font_size,
                 kTextColor, 0.f, false, true);
  DrawTextInRect(text.state_metadata, layout.state_metadata,
                 GetSecondaryFontSize(layout.font_size), kMutedTextColor, 0.f,
                 false, true, FontType::kSystemDefault);

  const bool previous_hovered =
      hovered_target_.type == HitTargetType::kStatePrevious;
  const bool next_hovered = hovered_target_.type == HitTargetType::kStateNext;
  const bool can_step_previous = CanStepState(StepDirection::kPrevious);
  const bool can_step_next = CanStepState(StepDirection::kNext);
  DrawButtonBackground(layout.state_previous, false, previous_hovered, false,
                       false, can_step_previous);
  DrawButtonBackground(layout.state_next, false, next_hovered, false, false,
                       can_step_next);
  DrawChevron(layout.state_previous, true,
              !can_step_previous
                  ? kMutedTextColor
                  : (previous_hovered ? kBrandColor : kTextColor));
  DrawChevron(layout.state_next, false,
              !can_step_next ? kMutedTextColor
                             : (next_hovered ? kBrandColor : kTextColor));
  draw_list->AddRect(RectMin(layout.state_position),
                     RectMax(layout.state_position), kBorderColor);
  DrawTextInRect(text.state_position, layout.state_position,
                 GetSecondaryFontSize(layout.font_size), kTextColor,
                 layout.padding / 2.f, true, true);

  const bool can_execute =
      focus_.menu_item == MenuItem::kSaveState ||
      state_preview_.status == StatePreview::Status::kReady;
  const bool action_selected =
      page_ == Page::kStateBrowser && focus_.area == FocusArea::kDetail;
  const bool action_hovered =
      hovered_target_.type == HitTargetType::kStateAction;
  DrawButtonBackground(layout.state_action, action_selected, action_hovered,
                       true, false, can_execute);
  DrawTextInRect(text.state_action, layout.state_action, layout.font_size,
                 can_execute ? kBrandOnColor : kMutedTextColor, layout.padding,
                 true, true);
}

void InGameMenu::DrawSettings(const FrameText& text,
                              const FrameLayout& layout) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->PushClipRect(RectMin(layout.detail), RectMax(layout.detail), true);
  for (size_t i = 0; i < kSettingsItemCount; ++i) {
    SettingsItem item = static_cast<SettingsItem>(i);
    const bool selected = page_ == Page::kSettings &&
                          focus_.area == FocusArea::kDetail &&
                          focus_.settings_item == item;
    const bool row_hovered =
        hovered_target_ ==
        HitTarget{HitTargetType::kSettingItem, static_cast<int>(i)};
    DrawButtonBackground(layout.setting_items[i], selected, row_hovered, false,
                         false);

    SDL_Rect label_rect =
        MakeRect(layout.setting_items[i].x, layout.setting_items[i].y,
                 layout.setting_previous[i].x - layout.setting_items[i].x -
                     layout.padding,
                 layout.setting_items[i].h);
    DrawTextInRect(text.setting_labels[i], label_rect, layout.font_size,
                   kTextColor, layout.padding, false, true);

    const bool previous_hovered =
        hovered_target_ ==
        HitTarget{HitTargetType::kSettingPrevious, static_cast<int>(i)};
    const bool next_hovered =
        hovered_target_ ==
        HitTarget{HitTargetType::kSettingNext, static_cast<int>(i)};
    const bool can_step_previous =
        CanStepSetting(item, StepDirection::kPrevious);
    const bool can_step_next = CanStepSetting(item, StepDirection::kNext);
    DrawButtonBackground(layout.setting_previous[i], false, previous_hovered,
                         false, false, can_step_previous);
    DrawButtonBackground(layout.setting_next[i], false, next_hovered, false,
                         false, can_step_next);
    DrawChevron(layout.setting_previous[i], true,
                !can_step_previous
                    ? kMutedTextColor
                    : (previous_hovered ? kBrandColor : kTextColor));
    DrawChevron(layout.setting_next[i], false,
                !can_step_next ? kMutedTextColor
                               : (next_hovered ? kBrandColor : kTextColor));

    draw_list->AddRectFilled(RectMin(layout.setting_values[i]),
                             RectMax(layout.setting_values[i]),
                             kPanelMutedColor);
    if (item == SettingsItem::kVolume) {
      float volume = std::clamp(runtime_data_->emulator->GetVolume(), 0.f, 1.f);
      SDL_Rect volume_fill = layout.setting_values[i];
      volume_fill.w = static_cast<int>(volume_fill.w * volume);
      draw_list->AddRectFilled(RectMin(volume_fill), RectMax(volume_fill),
                               IM_COL32(101, 216, 75, 76));
    }
    draw_list->AddRect(RectMin(layout.setting_values[i]),
                       RectMax(layout.setting_values[i]), kBorderColor);
    DrawTextInRect(text.setting_values[i], layout.setting_values[i],
                   GetSecondaryFontSize(layout.font_size), kTextColor,
                   layout.padding / 2.f, true, true, FontType::kSystemDefault);
  }
  if (layout.settings_content_height > layout.detail.h) {
    const float track_height =
        std::max(1.f, layout.detail.h - layout.padding * 2.f);
    const float thumb_height = std::max(
        24.f, track_height * layout.detail.h / layout.settings_content_height);
    const float max_scroll = layout.settings_content_height - layout.detail.h;
    const float thumb_y =
        layout.detail.y + layout.padding +
        (track_height - thumb_height) * settings_scroll_offset_ / max_scroll;
    const float thumb_x = RectRight(layout.detail) - 4.f;
    draw_list->AddRectFilled(ImVec2(thumb_x, thumb_y),
                             ImVec2(thumb_x + 2.f, thumb_y + thumb_height),
                             kMutedTextColor, 1.f);
  }
  draw_list->PopClipRect();
}

void InGameMenu::DrawConfirmation(const FrameText& text,
                                  const FrameLayout& layout) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddRectFilled(RectMin(layout.confirmation_body),
                           RectMax(layout.confirmation_body), kPanelMutedColor,
                           kCornerRadius);
  draw_list->AddRect(RectMin(layout.confirmation_body),
                     RectMax(layout.confirmation_body), kStrongBorderColor,
                     kCornerRadius);

  SDL_Rect title_rect =
      MakeRect(layout.confirmation_body.x + layout.padding,
               layout.confirmation_body.y + layout.padding,
               layout.confirmation_body.w - layout.padding * 2.f,
               layout.font_height * 1.75f);
  SDL_Rect message_rect = MakeRect(
      title_rect.x, RectBottom(title_rect), title_rect.w,
      layout.confirmation_cancel.y - RectBottom(title_rect) - layout.padding);
  DrawTextInRect(text.confirmation_title, title_rect, layout.font_size,
                 kTextColor, 0.f, false, true);
  DrawTextInRect(text.confirmation_message, message_rect,
                 GetSecondaryFontSize(layout.font_size), kMutedTextColor, 0.f,
                 false, true, FontType::kSystemDefault);

  const bool cancel_selected =
      page_ == Page::kConfirmation && !focus_.confirm_action;
  const bool confirm_selected =
      page_ == Page::kConfirmation && focus_.confirm_action;
  DrawButtonBackground(
      layout.confirmation_cancel, cancel_selected,
      hovered_target_.type == HitTargetType::kConfirmationCancel, false, false);
  DrawButtonBackground(
      layout.confirmation_accept, confirm_selected,
      hovered_target_.type == HitTargetType::kConfirmationAccept, false, true);
  DrawTextInRect(text.cancel, layout.confirmation_cancel, layout.font_size,
                 kTextColor, layout.padding, true, true);
  DrawTextInRect(text.confirm, layout.confirmation_accept, layout.font_size,
                 confirm_selected ? kTextColor : kDangerColor, layout.padding,
                 true, true);
}

void InGameMenu::DrawContextualAction(const FrameText& text,
                                      const FrameLayout& layout) {
  if (!IsValidRect(layout.contextual_action))
    return;

  const MenuItem item = focus_.menu_item;
  const bool primary = item == MenuItem::kContinue;
  const bool danger = item == MenuItem::kResetGame;
  const bool hovered = hovered_target_.type == HitTargetType::kContextualAction;
  DrawButtonBackground(layout.contextual_action, false, hovered, primary,
                       danger);
#if KIWI_MOBILE
  constexpr bool kWrapText = false;
#else
  constexpr bool kWrapText = true;
#endif
  DrawTextInRect(text.menu_items[ToIndex(item)], layout.contextual_action,
                 layout.font_size,
                 primary ? kBrandOnColor : (danger ? kDangerColor : kTextColor),
                 layout.padding, true, kWrapText);
}

bool InGameMenu::HandleInputEvent(SDL_KeyboardEvent* keyboard,
                                  SDL_ControllerButtonEvent* controller) {
  if (keyboard) {
    auto matches = [this, keyboard](kiwi::nes::ControllerButton button,
                                    SDL_KeyCode key) {
      return keyboard->keysym.sym == key ||
             IsJoystickButtonMatch(runtime_data_, button, keyboard->keysym);
    };
    if (matches(kiwi::nes::ControllerButton::kUp, SDLK_UP))
      return HandleNavigationAction(NavigationAction::kUp,
                                    InputModality::kKeyboard);
    if (matches(kiwi::nes::ControllerButton::kDown, SDLK_DOWN))
      return HandleNavigationAction(NavigationAction::kDown,
                                    InputModality::kKeyboard);
    if (matches(kiwi::nes::ControllerButton::kLeft, SDLK_LEFT))
      return HandleNavigationAction(NavigationAction::kLeft,
                                    InputModality::kKeyboard);
    if (matches(kiwi::nes::ControllerButton::kRight, SDLK_RIGHT))
      return HandleNavigationAction(NavigationAction::kRight,
                                    InputModality::kKeyboard);
    if (matches(kiwi::nes::ControllerButton::kA, SDLK_RETURN) ||
        keyboard->keysym.sym == SDLK_KP_ENTER) {
      return HandleNavigationAction(NavigationAction::kActivate,
                                    InputModality::kKeyboard);
    }
    if (matches(kiwi::nes::ControllerButton::kB, SDLK_ESCAPE))
      return HandleNavigationAction(NavigationAction::kBack,
                                    InputModality::kKeyboard);
    return false;
  }

  if (!controller)
    return false;
  switch (controller->button) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP:
      return HandleNavigationAction(NavigationAction::kUp,
                                    InputModality::kController);
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
      return HandleNavigationAction(NavigationAction::kDown,
                                    InputModality::kController);
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
      return HandleNavigationAction(NavigationAction::kLeft,
                                    InputModality::kController);
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
      return HandleNavigationAction(NavigationAction::kRight,
                                    InputModality::kController);
    case SDL_CONTROLLER_BUTTON_A:
      return HandleNavigationAction(NavigationAction::kActivate,
                                    InputModality::kController);
    case SDL_CONTROLLER_BUTTON_B:
    case SDL_CONTROLLER_BUTTON_X:
      return HandleNavigationAction(NavigationAction::kBack,
                                    InputModality::kController);
    default:
      return false;
  }
}

bool InGameMenu::HandleControllerAxis(SDL_ControllerAxisEvent* event) {
  int axis_index = -1;
  NavigationAction negative_action = NavigationAction::kLeft;
  NavigationAction positive_action = NavigationAction::kRight;
  if (event->axis == SDL_CONTROLLER_AXIS_LEFTX) {
    axis_index = 0;
  } else if (event->axis == SDL_CONTROLLER_AXIS_LEFTY) {
    axis_index = 1;
    negative_action = NavigationAction::kUp;
    positive_action = NavigationAction::kDown;
  } else {
    return false;
  }

  int direction = 0;
  if (event->value <= -kControllerDeadZone)
    direction = -1;
  else if (event->value >= kControllerDeadZone)
    direction = 1;

  if (direction == 0) {
    controller_axis_direction_[axis_index] = 0;
    return true;
  }

  const bool direction_changed =
      controller_axis_direction_[axis_index] != direction;
  if (!direction_changed &&
      controller_axis_repeat_timer_[axis_index].ElapsedInMilliseconds() <
          kControllerRepeatDelayMs) {
    return true;
  }

  controller_axis_direction_[axis_index] = direction;
  controller_axis_repeat_timer_[axis_index].Reset();
  return HandleNavigationAction(
      direction < 0 ? negative_action : positive_action,
      InputModality::kController);
}

bool InGameMenu::HandleNavigationAction(NavigationAction action,
                                        InputModality modality) {
  input_modality_ = modality;
  hovered_target_ = {};

  if (page_ == Page::kConfirmation) {
    if (action == NavigationAction::kLeft || action == NavigationAction::kUp) {
      focus_.confirm_action = false;
      PlayEffect(audio_resources::AudioID::kSelect);
    } else if (action == NavigationAction::kRight ||
               action == NavigationAction::kDown) {
      focus_.confirm_action = true;
      PlayEffect(audio_resources::AudioID::kSelect);
    } else if (action == NavigationAction::kActivate) {
      ExecuteConfirmation();
    } else if (action == NavigationAction::kBack) {
      PlayEffect(audio_resources::AudioID::kBack);
      ReturnToMenu();
    }
    return true;
  }

  if (page_ == Page::kStateBrowser) {
    if (action == NavigationAction::kLeft) {
      StepState(StepDirection::kPrevious);
    } else if (action == NavigationAction::kRight) {
      StepState(StepDirection::kNext);
    } else if (action == NavigationAction::kActivate) {
      ExecuteStateAction();
    } else if (action == NavigationAction::kBack ||
               action == NavigationAction::kUp) {
      PlayEffect(audio_resources::AudioID::kBack);
      ReturnToMenu();
    }
    return true;
  }

  if (page_ == Page::kSettings) {
    if (action == NavigationAction::kUp) {
      MoveSettingsSelection(-1);
    } else if (action == NavigationAction::kDown) {
      MoveSettingsSelection(1);
    } else if (action == NavigationAction::kLeft) {
      StepSetting(focus_.settings_item, StepDirection::kPrevious);
    } else if (action == NavigationAction::kRight ||
               action == NavigationAction::kActivate) {
      StepSetting(focus_.settings_item, StepDirection::kNext);
    } else if (action == NavigationAction::kBack) {
      PlayEffect(audio_resources::AudioID::kBack);
      ReturnToMenu();
    }
    return true;
  }

  if (action == NavigationAction::kUp) {
    MoveMenuSelection(-1);
  } else if (action == NavigationAction::kDown) {
    MoveMenuSelection(1);
  } else if (action == NavigationAction::kRight ||
             action == NavigationAction::kActivate) {
    ActivateMenuItem(focus_.menu_item);
  } else if (action == NavigationAction::kBack) {
    PlayEffect(audio_resources::AudioID::kBack);
    menu_callback_.Run(MenuCommand{MenuItem::kContinue, std::monostate{}});
  }
  return true;
}

void InGameMenu::ActivateMenuItem(MenuItem item) {
  focus_.menu_item = item;
  if (IsStateMenuItem(item)) {
    PlayEffect(audio_resources::AudioID::kSelect);
    OpenPage(Page::kStateBrowser);
  } else if (item == MenuItem::kOptions) {
    PlayEffect(audio_resources::AudioID::kSelect);
    OpenPage(Page::kSettings);
  } else if (item == MenuItem::kResetGame ||
             (item == MenuItem::kToGameSelection &&
              !IsStandaloneSettingsMenu())) {
    PlayEffect(audio_resources::AudioID::kSelect);
    confirmation_item_ = item;
    page_before_confirmation_ = page_;
    page_ = Page::kConfirmation;
    focus_.area = FocusArea::kConfirmation;
    focus_.confirm_action = false;
  } else {
    ExecuteMenuItem(item);
  }
}

void InGameMenu::ExecuteMenuItem(MenuItem item) {
  if (item == MenuItem::kToGameSelection)
    PlayEffect(audio_resources::AudioID::kBack);
  else
    PlayEffect(audio_resources::AudioID::kStart);
  menu_callback_.Run(MenuCommand{item, std::monostate{}});
}

void InGameMenu::ExecuteStateAction() {
  if (focus_.menu_item != MenuItem::kSaveState &&
      state_preview_.status != StatePreview::Status::kReady) {
    return;
  }

  PlayEffect(audio_resources::AudioID::kStart);
  if (focus_.menu_item == MenuItem::kLoadAutoSave) {
    menu_callback_.Run(MenuCommand{
        focus_.menu_item, AutoSaveTimestamp{state_preview_.timestamp}});
  } else {
    menu_callback_.Run(MenuCommand{focus_.menu_item, StateSlot{which_state_}});
  }
}

void InGameMenu::ExecuteConfirmation() {
  if (!focus_.confirm_action) {
    PlayEffect(audio_resources::AudioID::kBack);
    page_ = page_before_confirmation_;
    focus_.area =
        page_ == Page::kMainMenu ? FocusArea::kMenu : FocusArea::kDetail;
    return;
  }
  ExecuteMenuItem(confirmation_item_);
}

void InGameMenu::OpenPage(Page page) {
  page_ = page;
  focus_.area = page == Page::kMainMenu ? FocusArea::kMenu : FocusArea::kDetail;
  if (page == Page::kSettings)
    ScrollSettingsSelectionIntoView();
  if (page == Page::kStateBrowser &&
      state_preview_.status == StatePreview::Status::kEmpty) {
    RefreshStatePreview();
  }
}

void InGameMenu::ReturnToMenu() {
  page_ = Page::kMainMenu;
  focus_.area = FocusArea::kMenu;
  focus_.confirm_action = false;
}

void InGameMenu::MoveMenuSelection(int delta) {
  int selection = static_cast<int>(focus_.menu_item);
  for (size_t i = 0; i < kMenuItemCount; ++i) {
    selection = (selection + delta + static_cast<int>(kMenuItemCount)) %
                static_cast<int>(kMenuItemCount);
    MenuItem item = static_cast<MenuItem>(selection);
    if (IsMenuItemVisible(item)) {
      PlayEffect(audio_resources::AudioID::kSelect);
      MoveMenuItemTo(item);
      ScrollMenuSelectionIntoView();
      return;
    }
  }
}

void InGameMenu::MoveSettingsSelection(int delta) {
  int selection = static_cast<int>(focus_.settings_item);
  selection = (selection + delta + static_cast<int>(kSettingsItemCount)) %
              static_cast<int>(kSettingsItemCount);
  focus_.settings_item = static_cast<SettingsItem>(selection);
  PlayEffect(audio_resources::AudioID::kSelect);
  ScrollSettingsSelectionIntoView();
}

void InGameMenu::MoveMenuItemTo(MenuItem item) {
  if (!IsMenuItemVisible(item))
    return;
  if (focus_.menu_item == item)
    return;

  focus_.menu_item = item;
  if (item == MenuItem::kLoadAutoSave) {
    which_autosave_state_slot_ = 0;
    current_auto_states_count_ = 0;
    RequestAutoSavedStateCount();
    RefreshStatePreview();
  } else if (IsStateMenuItem(item)) {
    RefreshStatePreview();
  } else {
    ++auto_save_count_request_id_;
    CancelStatePreviewRequest();
  }
}

void InGameMenu::StepState(StepDirection direction) {
  if (!CanStepState(direction))
    return;

  if (focus_.menu_item == MenuItem::kLoadAutoSave) {
    const int last_slot = current_auto_states_count_ - 1;
    int next_slot = which_autosave_state_slot_;
    if (direction == StepDirection::kPrevious)
      next_slot = std::min(last_slot, next_slot + 1);
    else
      next_slot = std::max(0, next_slot - 1);
    if (next_slot == which_autosave_state_slot_)
      return;
    which_autosave_state_slot_ = next_slot;
  } else {
    const int delta = direction == StepDirection::kPrevious ? -1 : 1;
    which_state_ = (which_state_ + delta + NESRuntime::Data::MaxSaveStates) %
                   NESRuntime::Data::MaxSaveStates;
  }

  PlayEffect(audio_resources::AudioID::kSelect);
  RefreshStatePreview();
}

void InGameMenu::StepSetting(SettingsItem item, StepDirection direction) {
  if (!CanStepSetting(item, direction))
    return;

  focus_.settings_item = item;
  PlayEffect(audio_resources::AudioID::kSelect);
  settings_callback_.Run(item, direction == StepDirection::kPrevious);
}

void InGameMenu::SetVolumeFromPoint(float x, const SDL_Rect& bounds) {
  if (!IsValidRect(bounds))
    return;
  const float percentage =
      std::clamp((x - bounds.x) / static_cast<float>(bounds.w), 0.f, 1.f);
  focus_.settings_item = SettingsItem::kVolume;
  settings_callback_.Run(SettingsItem::kVolume, percentage);
}

InGameMenu::HitTarget InGameMenu::HitTest(const FrameLayout& layout,
                                          float x,
                                          float y) const {
  if (PointInRect(layout.close_button, x, y))
    return {HitTargetType::kClose};
  if (PointInRect(layout.back_button, x, y))
    return {HitTargetType::kBack};

  if (layout.visible_page == Page::kConfirmation) {
    if (PointInRect(layout.confirmation_cancel, x, y))
      return {HitTargetType::kConfirmationCancel};
    if (PointInRect(layout.confirmation_accept, x, y))
      return {HitTargetType::kConfirmationAccept};
    return {};
  }

  if (IsValidRect(layout.navigation)) {
    for (size_t i = 0; i < kMenuItemCount; ++i) {
      if (menu_item_visible_[i] &&
          PointInClippedRect(layout.menu_items[i], layout.navigation, x, y)) {
        return {HitTargetType::kMenuItem, static_cast<int>(i)};
      }
    }
  }

  if (layout.visible_page == Page::kStateBrowser) {
    if (CanStepState(StepDirection::kPrevious) &&
        PointInRect(layout.state_previous, x, y)) {
      return {HitTargetType::kStatePrevious};
    }
    if (CanStepState(StepDirection::kNext) &&
        PointInRect(layout.state_next, x, y)) {
      return {HitTargetType::kStateNext};
    }
    const bool can_execute =
        focus_.menu_item == MenuItem::kSaveState ||
        state_preview_.status == StatePreview::Status::kReady;
    if (can_execute && PointInRect(layout.state_action, x, y))
      return {HitTargetType::kStateAction};
  } else if (layout.visible_page == Page::kSettings) {
    for (size_t i = 0; i < kSettingsItemCount; ++i) {
      SettingsItem item = static_cast<SettingsItem>(i);
      if (CanStepSetting(item, StepDirection::kPrevious) &&
          PointInClippedRect(layout.setting_previous[i], layout.detail, x, y)) {
        return {HitTargetType::kSettingPrevious, static_cast<int>(i)};
      }
      if (CanStepSetting(item, StepDirection::kNext) &&
          PointInClippedRect(layout.setting_next[i], layout.detail, x, y)) {
        return {HitTargetType::kSettingNext, static_cast<int>(i)};
      }
      if (item == SettingsItem::kVolume &&
          PointInClippedRect(layout.setting_values[i], layout.detail, x, y)) {
        return {HitTargetType::kSettingValue, static_cast<int>(i)};
      }
      if (PointInClippedRect(layout.setting_items[i], layout.detail, x, y))
        return {HitTargetType::kSettingItem, static_cast<int>(i)};
    }
  } else if (PointInRect(layout.contextual_action, x, y)) {
    return {HitTargetType::kContextualAction};
  }
  return {};
}

void InGameMenu::UpdatePointerFocus(const HitTarget& target,
                                    InputModality modality) {
  input_modality_ = modality;
  hovered_target_ = target;
  if (target.type == HitTargetType::kMenuItem) {
    if (page_ != Page::kConfirmation)
      ReturnToMenu();
    MoveMenuItemTo(static_cast<MenuItem>(target.index));
  } else if (target.type == HitTargetType::kSettingItem ||
             target.type == HitTargetType::kSettingPrevious ||
             target.type == HitTargetType::kSettingValue ||
             target.type == HitTargetType::kSettingNext) {
    focus_.settings_item = static_cast<SettingsItem>(target.index);
  } else if (target.type == HitTargetType::kConfirmationCancel) {
    focus_.confirm_action = false;
  } else if (target.type == HitTargetType::kConfirmationAccept) {
    focus_.confirm_action = true;
  }
}

void InGameMenu::ActivateHitTarget(const HitTarget& target, float x) {
  input_modality_ = input_modality_ == InputModality::kTouch
                        ? InputModality::kTouch
                        : InputModality::kMouse;
  switch (target.type) {
    case HitTargetType::kBack:
      HandleNavigationAction(NavigationAction::kBack, input_modality_);
      break;
    case HitTargetType::kClose:
      PlayEffect(audio_resources::AudioID::kBack);
      menu_callback_.Run(MenuCommand{MenuItem::kContinue, std::monostate{}});
      break;
    case HitTargetType::kMenuItem: {
      MenuItem item = static_cast<MenuItem>(target.index);
      MoveMenuItemTo(item);
      ActivateMenuItem(item);
      break;
    }
    case HitTargetType::kStatePrevious:
      if (page_ == Page::kMainMenu)
        OpenPage(Page::kStateBrowser);
      StepState(StepDirection::kPrevious);
      break;
    case HitTargetType::kStateNext:
      if (page_ == Page::kMainMenu)
        OpenPage(Page::kStateBrowser);
      StepState(StepDirection::kNext);
      break;
    case HitTargetType::kStateAction:
      if (page_ == Page::kMainMenu)
        OpenPage(Page::kStateBrowser);
      ExecuteStateAction();
      break;
    case HitTargetType::kContextualAction:
      ActivateMenuItem(focus_.menu_item);
      break;
    case HitTargetType::kSettingItem:
      OpenPage(Page::kSettings);
      focus_.settings_item = static_cast<SettingsItem>(target.index);
      break;
    case HitTargetType::kSettingPrevious:
      if (page_ == Page::kMainMenu)
        OpenPage(Page::kSettings);
      StepSetting(static_cast<SettingsItem>(target.index),
                  StepDirection::kPrevious);
      break;
    case HitTargetType::kSettingValue: {
      if (page_ == Page::kMainMenu)
        OpenPage(Page::kSettings);
      SettingsItem item = static_cast<SettingsItem>(target.index);
      if (item == SettingsItem::kVolume)
        SetVolumeFromPoint(x, last_layout_.setting_values[target.index]);
      break;
    }
    case HitTargetType::kSettingNext:
      if (page_ == Page::kMainMenu)
        OpenPage(Page::kSettings);
      StepSetting(static_cast<SettingsItem>(target.index),
                  StepDirection::kNext);
      break;
    case HitTargetType::kConfirmationCancel:
      focus_.confirm_action = false;
      ExecuteConfirmation();
      break;
    case HitTargetType::kConfirmationAccept:
      focus_.confirm_action = true;
      ExecuteConfirmation();
      break;
    case HitTargetType::kNone:
      break;
  }
}

void InGameMenu::ClampMenuScroll(const FrameLayout& layout) {
  const float max_scroll =
      std::max(0.f, layout.menu_content_height - layout.navigation.h);
  menu_scroll_offset_ = std::clamp(menu_scroll_offset_, 0.f, max_scroll);
}

void InGameMenu::ClampSettingsScroll(const FrameLayout& layout) {
  const float max_scroll =
      std::max(0.f, layout.settings_content_height - layout.detail.h);
  settings_scroll_offset_ =
      std::clamp(settings_scroll_offset_, 0.f, max_scroll);
}

void InGameMenu::ScrollMenuSelectionIntoView() {
  if (!has_layout_ || last_layout_.mode != LayoutMode::kSinglePane)
    return;
  const SDL_Rect& item = last_layout_.menu_items[ToIndex(focus_.menu_item)];
  const float top = last_layout_.navigation.y + last_layout_.padding;
  const float bottom =
      RectBottom(last_layout_.navigation) - last_layout_.padding;
  if (item.y < top)
    menu_scroll_offset_ -= top - item.y;
  else if (RectBottom(item) > bottom)
    menu_scroll_offset_ += RectBottom(item) - bottom;
  ClampMenuScroll(last_layout_);
}

void InGameMenu::ScrollSettingsSelectionIntoView() {
  if (!has_layout_ || last_layout_.visible_page != Page::kSettings)
    return;
  const SDL_Rect& item =
      last_layout_.setting_items[ToIndex(focus_.settings_item)];
  const float top = last_layout_.detail.y + last_layout_.padding;
  const float bottom = RectBottom(last_layout_.detail) - last_layout_.padding;
  if (item.y < top)
    settings_scroll_offset_ -= top - item.y;
  else if (RectBottom(item) > bottom)
    settings_scroll_offset_ += RectBottom(item) - bottom;
  ClampSettingsScroll(last_layout_);
}

void InGameMenu::RefreshStatePreview() {
  ++state_preview_request_id_;
  state_preview_ = {};
  if (!IsStateMenuItem(focus_.menu_item))
    return;

  const kiwi::nes::RomData* rom_data = runtime_data_->emulator->GetRomData();
  if (!rom_data)
    return;

  state_preview_.status = StatePreview::Status::kLoading;
  const uint64_t request_id = state_preview_request_id_;
  std::weak_ptr<int> weak_lifetime = lifetime_token_;
  auto callback =
      kiwi::base::BindOnce(&InGameMenu::DispatchStateResult,
                           std::move(weak_lifetime), this, request_id);
  if (focus_.menu_item == MenuItem::kLoadAutoSave) {
    runtime_data_->GetAutoSavedState(rom_data->crc, which_autosave_state_slot_,
                                     std::move(callback));
  } else {
    runtime_data_->GetState(rom_data->crc, which_state_, std::move(callback));
  }
}

void InGameMenu::RequestAutoSavedStateCount() {
  ++auto_save_count_request_id_;
  const kiwi::nes::RomData* rom_data = runtime_data_->emulator->GetRomData();
  if (!rom_data) {
    current_auto_states_count_ = 0;
    return;
  }

  const uint64_t request_id = auto_save_count_request_id_;
  std::weak_ptr<int> weak_lifetime = lifetime_token_;
  runtime_data_->GetAutoSavedStatesCount(
      rom_data->crc,
      kiwi::base::BindOnce(&InGameMenu::DispatchAutoSavedStateCount,
                           std::move(weak_lifetime), this, request_id));
}

void InGameMenu::DispatchAutoSavedStateCount(std::weak_ptr<int> weak_lifetime,
                                             InGameMenu* menu,
                                             uint64_t request_id,
                                             int count) {
  if (!weak_lifetime.expired())
    menu->OnGotAutoSavedStateCount(request_id, count);
}

void InGameMenu::DispatchStateResult(
    std::weak_ptr<int> weak_lifetime,
    InGameMenu* menu,
    uint64_t request_id,
    const NESRuntime::Data::StateResult& result) {
  if (!weak_lifetime.expired())
    menu->OnGotState(request_id, result);
}

void InGameMenu::OnGotAutoSavedStateCount(uint64_t request_id, int count) {
  if (request_id != auto_save_count_request_id_)
    return;
  current_auto_states_count_ = std::max(0, count);
  const int last_slot = std::max(0, current_auto_states_count_ - 1);
  if (which_autosave_state_slot_ > last_slot) {
    which_autosave_state_slot_ = last_slot;
    if (focus_.menu_item == MenuItem::kLoadAutoSave)
      RefreshStatePreview();
  }
}

void InGameMenu::OnGotState(uint64_t request_id,
                            const NESRuntime::Data::StateResult& state_result) {
  if (request_id != state_preview_request_id_)
    return;

  state_preview_.status = StatePreview::Status::kEmpty;
  state_preview_.has_thumbnail = false;
  state_preview_.timestamp = 0;
  if (!state_result.success || state_result.state_data.empty())
    return;

  state_preview_.status = StatePreview::Status::kReady;
  state_preview_.timestamp = state_result.slot_or_timestamp;
  if (state_result.thumbnail_data.empty())
    return;

  if (!snapshot_) {
    snapshot_ = SDL_CreateTexture(
        window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, Canvas::kNESFrameDefaultWidth,
        Canvas::kNESFrameDefaultHeight);
  }
  if (!snapshot_)
    return;

  constexpr int kColorComponents = 4;
  if (SDL_UpdateTexture(snapshot_, nullptr, state_result.thumbnail_data.data(),
                        Canvas::kNESFrameDefaultWidth * kColorComponents *
                            sizeof(state_result.thumbnail_data.data()[0])) ==
      0) {
    state_preview_.has_thumbnail = true;
  }
}

void InGameMenu::CancelStatePreviewRequest() {
  ++state_preview_request_id_;
  state_preview_ = {};
}

void InGameMenu::SetFirstSelection() {
  for (size_t i = 0; i < kMenuItemCount; ++i) {
    if (menu_item_visible_[i]) {
      focus_.menu_item = static_cast<MenuItem>(i);
      return;
    }
  }
}

bool InGameMenu::IsMenuItemVisible(MenuItem item) const {
  return menu_item_visible_[ToIndex(item)];
}

bool InGameMenu::IsStateMenuItem(MenuItem item) const {
  return item == MenuItem::kLoadAutoSave || item == MenuItem::kLoadState ||
         item == MenuItem::kSaveState;
}

bool InGameMenu::IsStandaloneSettingsMenu() const {
  return !IsMenuItemVisible(MenuItem::kContinue) &&
         !IsMenuItemVisible(MenuItem::kLoadAutoSave) &&
         !IsMenuItemVisible(MenuItem::kLoadState) &&
         !IsMenuItemVisible(MenuItem::kSaveState) &&
         IsMenuItemVisible(MenuItem::kOptions) &&
         !IsMenuItemVisible(MenuItem::kResetGame);
}

bool InGameMenu::CanStepState(StepDirection direction) const {
  if (focus_.menu_item != MenuItem::kLoadAutoSave)
    return IsStateMenuItem(focus_.menu_item);
  if (current_auto_states_count_ <= 0)
    return false;
  return direction == StepDirection::kPrevious
             ? which_autosave_state_slot_ < current_auto_states_count_ - 1
             : which_autosave_state_slot_ > 0;
}

bool InGameMenu::CanStepSetting(SettingsItem item,
                                StepDirection direction) const {
  switch (item) {
    case SettingsItem::kVolume: {
      const float volume = runtime_data_->emulator->GetVolume();
      return direction == StepDirection::kPrevious ? volume > 0.f
                                                   : volume < 1.f;
    }
    case SettingsItem::kWindowMode:
#if !KIWI_MOBILE
#if KIWI_WASM
      return false;
#else
      return direction == StepDirection::kPrevious
                 ? main_window_->is_fullscreen()
                 : !main_window_->is_fullscreen();
#endif
#else
      return true;
#endif
    case SettingsItem::kJoyP1:
    case SettingsItem::kJoyP2: {
      const int player = item == SettingsItem::kJoyP1 ? 0 : 1;
      std::vector<SDL_GameController*> controllers = GetControllerList();
      auto* current = reinterpret_cast<SDL_GameController*>(
          runtime_data_->joystick_mappings[player].which);
      auto current_iter =
          std::find(controllers.begin(), controllers.end(), current);
      if (current_iter == controllers.end())
        current_iter = controllers.begin();
      return direction == StepDirection::kPrevious
                 ? current_iter != controllers.begin()
                 : current_iter + 1 != controllers.end();
    }
    case SettingsItem::kSwapABP1:
    case SettingsItem::kSwapABP2: {
      const int player = item == SettingsItem::kSwapABP1 ? 0 : 1;
      const bool swapped = main_window_->IsABSwapped(player);
      return direction == StepDirection::kPrevious ? swapped : !swapped;
    }
    case SettingsItem::kLanguage:
      return static_cast<int>(SupportedLanguage::kMax) > 1;
    case SettingsItem::kMax:
      return false;
  }
  return false;
}

InGameMenu::Page InGameMenu::GetPreviewPage() const {
  if (IsStateMenuItem(focus_.menu_item))
    return Page::kStateBrowser;
  if (focus_.menu_item == MenuItem::kOptions)
    return Page::kSettings;
  return Page::kMainMenu;
}

std::string InGameMenu::GetSettingValue(SettingsItem item) const {
  switch (item) {
    case SettingsItem::kVolume: {
      char buffer[16] = {};
      std::snprintf(
          buffer, sizeof(buffer), "%d%%",
          static_cast<int>(std::round(
              std::clamp(runtime_data_->emulator->GetVolume(), 0.f, 1.f) *
              100.f)));
      return buffer;
    }
    case SettingsItem::kWindowMode:
#if !KIWI_MOBILE
      if (main_window_->is_fullscreen()) {
        return GetLocalizedString(
            string_resources::IDR_IN_GAME_MENU_FULLSCREEN);
      }
      return GetLocalizedString(string_resources::IDR_IN_GAME_MENU_WINDOWED);
#else
      return GetLocalizedString(
          main_window_->is_stretch_mode()
              ? string_resources::IDR_IN_GAME_MENU_STRETCH
              : string_resources::IDR_IN_GAME_MENU_ORIGINAL);
#endif
    case SettingsItem::kJoyP1:
    case SettingsItem::kJoyP2: {
      const int player = item == SettingsItem::kJoyP1 ? 0 : 1;
      auto* controller = reinterpret_cast<SDL_GameController*>(
          runtime_data_->joystick_mappings[player].which);
      std::vector<SDL_GameController*> controllers = GetControllerList();
      auto controller_iter =
          std::find(controllers.begin(), controllers.end(), controller);
      const char* controller_name =
          controller && controller_iter != controllers.end()
              ? SDL_GameControllerName(controller)
              : nullptr;
      return controller_name
                 ? controller_name
                 : GetLocalizedString(string_resources::IDR_IN_GAME_MENU_NONE);
    }
    case SettingsItem::kSwapABP1:
    case SettingsItem::kSwapABP2: {
      const int player = item == SettingsItem::kSwapABP1 ? 0 : 1;
      return GetLocalizedString(main_window_->IsABSwapped(player)
                                    ? string_resources::IDR_IN_GAME_MENU_ON
                                    : string_resources::IDR_IN_GAME_MENU_OFF);
    }
    case SettingsItem::kLanguage:
      switch (GetCurrentSupportedLanguage()) {
#if !DISABLE_CHINESE_FONT
        case SupportedLanguage::kSimplifiedChinese:
          return GetLocalizedString(
              string_resources::IDR_IN_GAME_MENU_LANGUAGE_ZH);
#endif
#if !DISABLE_JAPANESE_FONT
        case SupportedLanguage::kJapanese:
          return GetLocalizedString(
              string_resources::IDR_IN_GAME_MENU_LANGUAGE_JP);
#endif
        case SupportedLanguage::kEnglish:
        default:
          return GetLocalizedString(
              string_resources::IDR_IN_GAME_MENU_LANGUAGE_EN);
      }
    case SettingsItem::kMax:
      break;
  }
  return std::string();
}
