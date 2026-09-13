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

#include "ui/main_window.h"

#include <SDL.h>

#include <algorithm>
#include <cfloat>
#include <optional>
#include <string>

#include "resources/string_resources.h"
#include "ui/styles.h"
#include "ui/widgets/canvas.h"
#include "ui/widgets/joystick_button.h"
#include "ui/widgets/touch_button.h"
#include "ui/widgets/virtual_joystick.h"
#include "utility/fonts.h"
#include "utility/localization.h"
#include "utility/math.h"

#if KIWI_ANDROID
#include "third_party/SDL2/src/core/android/SDL_android.h"
#endif

namespace {
constexpr int kDefaultWindowWidth = Canvas::kNESFrameDefaultWidth;
constexpr int kDefaultWindowHeight = Canvas::kNESFrameDefaultHeight;
constexpr int kCenterButtonWidth = 88;
constexpr int kCenterButtonHeight = 34;
constexpr float kCenterButtonOpacity = .28f;

constexpr int kHDTextureToggleWidth = 128;
constexpr int kHDTextureToggleHeight = 32;
constexpr int kHDTextureToggleRightMargin = 12;
constexpr int kHDTextureToggleTopMargin = 10;
constexpr ImU32 kHDTextureToggleFillColor = IM_COL32(24, 29, 27, 255);
constexpr ImU32 kHDTextureToggleBorderColor = IM_COL32(143, 153, 147, 255);
constexpr ImU32 kHDTextureToggleAccentColor = IM_COL32(19, 218, 255, 255);
constexpr ImU32 kHDTextureToggleTextColor = IM_COL32(245, 247, 245, 255);

float GetPresentationScale(NESFrame& frame, float logical_scale) {
  if (frame.width() <= 0 || frame.height() <= 0) {
    return logical_scale;
  }

  const float source_scale =
      std::max(static_cast<float>(frame.width()) / kDefaultWindowWidth,
               static_cast<float>(frame.height()) / kDefaultWindowHeight);
  return logical_scale / source_scale;
}

ImU32 ColorWithOpacity(ImU32 color, float opacity) {
  const ImU32 alpha = static_cast<ImU32>(std::clamp(opacity, 0.f, 1.f) * 255.f);
  return (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
}

void DrawCenteredText(ImDrawList* draw_list,
                      ImFont* font,
                      float font_size,
                      const std::string& text,
                      const ImVec2& minimum,
                      const ImVec2& maximum,
                      ImU32 color) {
  const ImVec2 text_size =
      font->CalcTextSizeA(font_size, FLT_MAX, 0.f, text.c_str());
  draw_list->AddText(
      font, font_size,
      ImVec2(minimum.x + (maximum.x - minimum.x - text_size.x) * .5f,
             minimum.y + (maximum.y - minimum.y - text_size.y) * .5f),
      color, text.c_str());
}

class HDTextureToggle final : public Widget {
 public:
  using SelectionCallback = kiwi::base::RepeatingCallback<void(bool)>;

  explicit HDTextureToggle(MainWindow* window) : Widget(window) {
    set_flags(ImGuiWindowFlags_NoDecoration |
              ImGuiWindowFlags_AlwaysAutoResize |
              ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
    set_title("##HDTextureToggle");
  }
  ~HDTextureToggle() override = default;

  void SetState(bool visible, bool hd_enabled) {
    hd_enabled_ = hd_enabled;
    set_visible(visible);
    if (!visible) {
      active_finger_.reset();
      pressed_ = false;
    }
  }
  void set_selection_callback(SelectionCallback callback) {
    selection_callback_ = std::move(callback);
  }

 protected:
  void Paint() override {
    const SDL_Rect mapped_bounds = MapToWindow(bounds());
    const ImVec2 minimum(mapped_bounds.x, mapped_bounds.y);
    const ImVec2 maximum(mapped_bounds.x + mapped_bounds.w,
                         mapped_bounds.y + mapped_bounds.h);
    const float opacity = pressed_ ? 1.f : kCenterButtonOpacity;
    const float control_scale = std::max(
        .5f, static_cast<float>(mapped_bounds.h) / kHDTextureToggleHeight);
    const float rounding = std::max(4.f, 8.f * control_scale);
    const float border_width = std::max(2.f, 2.f * control_scale);
    const float half_width = mapped_bounds.w * .5f;
    const bool highlighted_hd = pressed_ ? pressed_hd_ : hd_enabled_;
    const ImVec2 selected_min(highlighted_hd
                                  ? minimum.x + border_width
                                  : minimum.x + half_width + border_width,
                              minimum.y + border_width);
    const ImVec2 selected_max(highlighted_hd
                                  ? minimum.x + half_width - border_width
                                  : maximum.x - border_width,
                              maximum.y - border_width);

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();
    draw_list->AddRectFilled(
        minimum, maximum,
        ColorWithOpacity(kHDTextureToggleFillColor, opacity * .82f), rounding);
    draw_list->AddRectFilled(
        selected_min, selected_max,
        ColorWithOpacity(kHDTextureToggleAccentColor, opacity * .52f),
        std::max(2.f, rounding - border_width));
    draw_list->AddRect(minimum, maximum,
                       ColorWithOpacity(kHDTextureToggleBorderColor, opacity),
                       rounding, 0, border_width);
    draw_list->AddLine(
        ImVec2(minimum.x + half_width, minimum.y + border_width),
        ImVec2(minimum.x + half_width, maximum.y - border_width),
        ColorWithOpacity(kHDTextureToggleBorderColor, opacity * .8f),
        border_width);

    const std::string& hd_text =
        GetLocalizedString(string_resources::IDR_HD_TEXTURE_MODE_HD);
    const std::string& original_text =
        GetLocalizedString(string_resources::IDR_HD_TEXTURE_MODE_ORIGINAL);
    const std::string text_hint = hd_text + original_text;
    ScopedFont preferred_font = GetPreferredFont(
        PreferredFontSize::k4x, text_hint.c_str(), FontType::kSystemDefault);
    ImFont* font = preferred_font.GetFont();
    float font_size =
        std::min(preferred_font.GetFontSize(), mapped_bounds.h * .46f);
    const float maximum_text_width =
        half_width - std::max(12.f, 16.f * control_scale);
    const ImVec2 hd_text_size =
        font->CalcTextSizeA(font_size, FLT_MAX, 0.f, hd_text.c_str());
    const ImVec2 original_text_size =
        font->CalcTextSizeA(font_size, FLT_MAX, 0.f, original_text.c_str());
    const float widest_text = std::max(hd_text_size.x, original_text_size.x);
    if (widest_text > maximum_text_width) {
      font_size *= maximum_text_width / widest_text;
    }

    const ImU32 active_text_color =
        ColorWithOpacity(kHDTextureToggleTextColor, opacity);
    const ImU32 inactive_text_color =
        ColorWithOpacity(kHDTextureToggleTextColor, opacity * .62f);
    DrawCenteredText(draw_list, font, font_size, hd_text, minimum,
                     ImVec2(minimum.x + half_width, maximum.y),
                     highlighted_hd ? active_text_color : inactive_text_color);
    DrawCenteredText(draw_list, font, font_size, original_text,
                     ImVec2(minimum.x + half_width, minimum.y), maximum,
                     highlighted_hd ? inactive_text_color : active_text_color);
  }

  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override {
    const SDL_Point point = GetTouchPoint(event);
    const SDL_Rect mapped_bounds = MapToWindow(bounds());
    if (!Contains(mapped_bounds, point.x, point.y)) {
      return false;
    }
    active_finger_ = event->fingerId;
    pressed_ = true;
    pressed_hd_ = point.x < mapped_bounds.x + mapped_bounds.w / 2;
    return true;
  }

  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override {
    if (!active_finger_ || *active_finger_ != event->fingerId) {
      return false;
    }

    const SDL_Point point = GetTouchPoint(event);
    const SDL_Rect mapped_bounds = MapToWindow(bounds());
    const bool released_inside = Contains(mapped_bounds, point.x, point.y);
    const bool selected_hd = point.x < mapped_bounds.x + mapped_bounds.w / 2;
    active_finger_.reset();
    pressed_ = false;
    if (released_inside && selected_hd != hd_enabled_ && selection_callback_) {
      selection_callback_.Run(selected_hd);
    }
    return true;
  }

  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override {
    if (!active_finger_ || *active_finger_ != event->fingerId) {
      return false;
    }
    const SDL_Point point = GetTouchPoint(event);
    const SDL_Rect mapped_bounds = MapToWindow(bounds());
    pressed_ = Contains(mapped_bounds, point.x, point.y);
    if (pressed_) {
      pressed_hd_ = point.x < mapped_bounds.x + mapped_bounds.w / 2;
    }
    return true;
  }

  int GetHitTestPolicy() override {
    return Widget::GetHitTestPolicy() | kAlwaysHitTest;
  }

 private:
  SDL_Point GetTouchPoint(SDL_TouchFingerEvent* event) {
    const SDL_Rect client_bounds = window()->GetClientBounds();
    return SDL_Point{static_cast<int>(event->x * client_bounds.w),
                     static_cast<int>(event->y * client_bounds.h)};
  }

  SelectionCallback selection_callback_;
  std::optional<SDL_FingerID> active_finger_;
  bool hd_enabled_ = true;
  bool pressed_ = false;
  bool pressed_hd_ = true;
};
}  // namespace

bool MainWindow::IsLandscape() {
  const SDL_Rect kClientBounds = GetClientBounds();
  return kClientBounds.w > kClientBounds.h;
}

void MainWindow::CreateVirtualTouchButtons() {
#if KIWI_ANDROID
  // TV applications uses joysticks or remote controller.
  if (SDL_IsAndroidTV())
    return;
#endif

  {
    std::unique_ptr<VirtualJoystick> vtb_joystick =
        std::make_unique<VirtualJoystick>(this);
    vtb_joystick_ = vtb_joystick.get();
    vtb_joystick->set_visible(false);
    vtb_joystick->set_joystick_callback(kiwi::base::BindRepeating(
        &MainWindow::OnVirtualJoystickChanged, kiwi::base::Unretained(this)));
    AddWidget(std::move(vtb_joystick));
  }

  {
    std::unique_ptr<JoystickButton> vtb_a =
        std::make_unique<JoystickButton>(this, image_resources::ImageID::kVtbA,
                                         TouchButton::VisualStyle::kActionA);
    vtb_a_ = vtb_a.get();
    vtb_a->set_finger_down_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kA, true));
    vtb_a->set_trigger_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kA, false));
    vtb_a->set_visible(false);
    AddWidget(std::move(vtb_a));
  }

  {
    std::unique_ptr<JoystickButton> vtb_b =
        std::make_unique<JoystickButton>(this, image_resources::ImageID::kVtbB,
                                         TouchButton::VisualStyle::kActionB);
    vtb_b_ = vtb_b.get();
    vtb_b->set_finger_down_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kB, true));
    vtb_b->set_trigger_callback(kiwi::base::BindRepeating(
        &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this), 0,
        kiwi::nes::ControllerButton::kB, false));
    vtb_b->set_visible(false);
    AddWidget(std::move(vtb_b));
  }

  {
    std::unique_ptr<JoystickButton> vtb_ab =
        std::make_unique<JoystickButton>(this, image_resources::ImageID::kVtbAb,
                                         TouchButton::VisualStyle::kActionAB);
    vtb_ab_ = vtb_ab.get();
    vtb_ab->set_finger_down_callback(
        kiwi::base::BindRepeating(&MainWindow::SetVirtualJoystickButton,
                                  kiwi::base::Unretained(this), 0,
                                  kiwi::nes::ControllerButton::kA, true)
            .Then(kiwi::base::BindRepeating(
                &MainWindow::SetVirtualJoystickButton,
                kiwi::base::Unretained(this), 0,
                kiwi::nes::ControllerButton::kB, true)));
    vtb_ab->set_trigger_callback(
        kiwi::base::BindRepeating(&MainWindow::SetVirtualJoystickButton,
                                  kiwi::base::Unretained(this), 0,
                                  kiwi::nes::ControllerButton::kA, false)
            .Then(kiwi::base::BindRepeating(
                &MainWindow::SetVirtualJoystickButton,
                kiwi::base::Unretained(this), 0,
                kiwi::nes::ControllerButton::kB, false)));
    vtb_ab->set_visible(false);
    AddWidget(std::move(vtb_ab));
  }

  {
    {
      std::unique_ptr<TouchButton> vtb_select = std::make_unique<TouchButton>(
          this, image_resources::ImageID::kVtbSelect,
          TouchButton::VisualStyle::kSelect);
      vtb_select_ = vtb_select.get();
      vtb_select->set_finger_down_callback(kiwi::base::BindRepeating(
          &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this),
          0, kiwi::nes::ControllerButton::kSelect, true));
      vtb_select->set_trigger_callback(kiwi::base::BindRepeating(
          &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this),
          0, kiwi::nes::ControllerButton::kSelect, false));
      SDL_Rect bounds = vtb_select->bounds();
      bounds.w = kCenterButtonWidth * window_scale();
      bounds.h = kCenterButtonHeight * window_scale();
      vtb_select->set_opacity(kCenterButtonOpacity);
      vtb_select->set_bounds(bounds);
      vtb_select->set_visible(false);
      AddWidget(std::move(vtb_select));
    }

    {
      std::unique_ptr<TouchButton> vtb_start = std::make_unique<TouchButton>(
          this, image_resources::ImageID::kVtbStart,
          TouchButton::VisualStyle::kStart);
      vtb_start_ = vtb_start.get();
      vtb_start->set_finger_down_callback(kiwi::base::BindRepeating(
          &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this),
          0, kiwi::nes::ControllerButton::kStart, true));
      vtb_start->set_trigger_callback(kiwi::base::BindRepeating(
          &MainWindow::SetVirtualJoystickButton, kiwi::base::Unretained(this),
          0, kiwi::nes::ControllerButton::kStart, false));
      SDL_Rect bounds = vtb_start->bounds();
      bounds.w = kCenterButtonWidth * window_scale();
      bounds.h = kCenterButtonHeight * window_scale();
      vtb_start->set_opacity(kCenterButtonOpacity);
      vtb_start->set_bounds(bounds);
      vtb_start->set_visible(false);
      AddWidget(std::move(vtb_start));
    }

    {
      std::unique_ptr<TouchButton> vtb_pause = std::make_unique<TouchButton>(
          this, image_resources::ImageID::kVtbPause,
          TouchButton::VisualStyle::kPause);
      vtb_pause_ = vtb_pause.get();
      vtb_pause->set_trigger_callback(kiwi::base::BindRepeating(
          &MainWindow::OnInGameMenuTrigger, kiwi::base::Unretained(this)));
      vtb_pause->set_visible(false);
      AddWidget(std::move(vtb_pause));
    }

    {
      std::unique_ptr<HDTextureToggle> hd_texture_toggle =
          std::make_unique<HDTextureToggle>(this);
      hd_texture_toggle_ = hd_texture_toggle.get();
      hd_texture_toggle->set_selection_callback(
          kiwi::base::BindRepeating(&MainWindow::OnSetHDTextureRenderingEnabled,
                                    kiwi::base::Unretained(this)));
      hd_texture_toggle->set_bounds(SDL_Rect{
          0, 0, static_cast<int>(kHDTextureToggleWidth * window_scale()),
          static_cast<int>(kHDTextureToggleHeight * window_scale())});
      hd_texture_toggle->SetZOrder(1);
      hd_texture_toggle->set_visible(false);
      AddWidget(std::move(hd_texture_toggle));
    }
  }
}

void MainWindow::SetVirtualTouchButtonVisible(VirtualTouchButton button,
                                              bool visible) {
#if KIWI_ANDROID
  // TV applications uses joysticks or remote controller.
  if (SDL_IsAndroidTV())
    return;
#endif

  switch (button) {
    case VirtualTouchButton::kStart:
      if (vtb_start_)
        vtb_start_->set_visible(visible);
      break;
    case VirtualTouchButton::kSelect:
      if (vtb_select_)
        vtb_select_->set_visible(visible);
      break;
    case VirtualTouchButton::kJoystick:
      if (vtb_joystick_)
        vtb_joystick_->set_visible(visible);
      break;
    case VirtualTouchButton::kA:
      if (vtb_a_)
        vtb_a_->set_visible(visible);
      break;
    case VirtualTouchButton::kB:
      if (vtb_b_)
        vtb_b_->set_visible(visible);
      break;
    case VirtualTouchButton::kAB:
      if (vtb_ab_)
        vtb_ab_->set_visible(visible);
      break;
    case VirtualTouchButton::kPause:
      if (vtb_pause_)
        vtb_pause_->set_visible(visible);
      break;
    default:
      SDL_assert(false);
      break;
  }
}

void MainWindow::SetHDTextureToggleState(bool visible, bool hd_enabled) {
  if (!hd_texture_toggle_) {
    return;
  }
  HDTextureToggle* toggle = static_cast<HDTextureToggle*>(hd_texture_toggle_);
  toggle->SetState(visible, hd_enabled);
}

void MainWindow::LayoutVirtualTouchButtons() {
#if KIWI_ANDROID
  // TV applications uses joysticks or remote controller.
  if (SDL_IsAndroidTV())
    return;
#endif

  const SDL_Rect safe_bounds = GetSafeAreaClientBounds();
  const SDL_Rect no_safe_area_insets = {};
  bool is_landscape = IsLandscape();

  {
    const int kSize = styles::main_window::GetJoystickSize(window_scale());
    const int kPaddingX = styles::main_window::GetJoystickMarginX(
        window_scale(), is_landscape, no_safe_area_insets);
    const int kPaddingY = styles::main_window::GetJoystickMarginY(
        window_scale(), is_landscape, no_safe_area_insets);

    if (vtb_joystick_) {
      SDL_Rect bounds;
      bounds.h = bounds.w = kSize;
      bounds.x = safe_bounds.x + kPaddingX;
      bounds.y = safe_bounds.y + safe_bounds.h - bounds.h - kPaddingY;
      vtb_joystick_->set_bounds(bounds);
    }
  }

  {
    const int kSize = 76 * window_scale();
    const int kComboSize = 64 * window_scale();
    const int kPaddingX = styles::main_window::GetJoystickButtonMarginX(
        window_scale(), is_landscape, no_safe_area_insets);
    const int kPaddingY = styles::main_window::GetJoystickButtonMarginY(
        window_scale(), is_landscape, no_safe_area_insets);
    const int kSpacing = 6 * window_scale();
    const int safe_right = safe_bounds.x + safe_bounds.w;
    const int action_bottom = safe_bounds.y + safe_bounds.h - kPaddingY;
    SDL_Rect right_bounds;
    right_bounds.h = right_bounds.w = kSize;
    right_bounds.x = safe_right - right_bounds.w - kPaddingX;
    right_bounds.y = action_bottom - right_bounds.h;

    SDL_Rect left_bounds = right_bounds;
    left_bounds.x = safe_right - left_bounds.w * 2 - kPaddingX - kSpacing;
    const bool swap_ab = IsABSwapped(0);
    if (vtb_a_) {
      vtb_a_->set_bounds(swap_ab ? left_bounds : right_bounds);
    }

    if (vtb_b_) {
      vtb_b_->set_bounds(swap_ab ? right_bounds : left_bounds);
    }

    if (vtb_ab_) {
      SDL_Rect bounds;
      bounds.h = bounds.w = kComboSize;
      const int action_center_x = safe_right - kPaddingX - kSize / 2;
      bounds.x = action_center_x - bounds.w / 2;
      bounds.y = action_bottom - kSize - bounds.h - kSpacing;
      vtb_ab_->set_bounds(bounds);
    }
  }

  {
    const int kMiddleSpacing = 4 * window_scale();
    const int kButtonWidth = kCenterButtonWidth * window_scale();
    const int kButtonHeight = kCenterButtonHeight * window_scale();
    const int kPaddingBottom =
        styles::main_window::GetJoystickSelectStartButtonMarginBottom(
            window_scale(), is_landscape, no_safe_area_insets);
    const int safe_center_x = safe_bounds.x + safe_bounds.w / 2;
    if (vtb_select_) {
      SDL_Rect bounds = vtb_select_->bounds();
      bounds.w = kButtonWidth;
      bounds.h = kButtonHeight;
      bounds.x = safe_center_x - bounds.w - kMiddleSpacing;
      bounds.y = safe_bounds.y + safe_bounds.h - bounds.h - kPaddingBottom;
      vtb_select_->set_bounds(bounds);
    }

    if (vtb_start_) {
      SDL_Rect bounds = vtb_start_->bounds();
      bounds.w = kButtonWidth;
      bounds.h = kButtonHeight;
      bounds.x = safe_center_x + kMiddleSpacing;
      bounds.y = safe_bounds.y + safe_bounds.h - bounds.h - kPaddingBottom;
      vtb_start_->set_bounds(bounds);
    }
  }

  if (vtb_pause_) {
    const int kPaddingX = styles::main_window::GetJoystickPauseButtonMarginX(
        window_scale(), no_safe_area_insets);
    const int kPaddingY = styles::main_window::GetJoystickPauseButtonMarginY(
        window_scale(), no_safe_area_insets);
    const int kSize = 42 * window_scale();
    SDL_Rect bounds;
    bounds.h = bounds.w = kSize;
    bounds.x = safe_bounds.x + kPaddingX;
    bounds.y = safe_bounds.y + kPaddingY;
    vtb_pause_->set_bounds(bounds);
  }

  if (hd_texture_toggle_) {
    SDL_Rect bounds;
    bounds.w = kHDTextureToggleWidth * window_scale();
    bounds.h = kHDTextureToggleHeight * window_scale();
    bounds.x = safe_bounds.x + safe_bounds.w - bounds.w -
               kHDTextureToggleRightMargin * window_scale();
    bounds.y = safe_bounds.y + kHDTextureToggleTopMargin * window_scale();
    hd_texture_toggle_->set_bounds(bounds);
  }
}

void MainWindow::OnVirtualJoystickChanged(int state) {
#if KIWI_ANDROID
  // TV applications uses joysticks or remote controller.
  if (SDL_IsAndroidTV())
    return;
#endif

  SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kLeft, false);
  SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kRight, false);
  SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kUp, false);
  SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kDown, false);

  if (state & VirtualJoystick::kLeft)
    SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kLeft, true);
  if (state & VirtualJoystick::kRight)
    SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kRight, true);
  if (state & VirtualJoystick::kUp)
    SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kUp, true);
  if (state & VirtualJoystick::kDown)
    SetVirtualJoystickButton(0, kiwi::nes::ControllerButton::kDown, true);
}

void MainWindow::OnInGameSettingsHandleWindowMode(bool is_left) {
  if (config_->data().is_stretch_mode && !is_left)
    return;

  if (config_->data().is_stretch_mode && is_left) {
    config_->data().is_stretch_mode = false;
    config_->SaveConfig();
    OnScaleModeChanged();
  } else if (!config_->data().is_stretch_mode && !is_left) {
    config_->data().is_stretch_mode = true;
    config_->SaveConfig();
    OnScaleModeChanged();
  }
}

void MainWindow::OnScaleModeChanged() {
  if (canvas_) {
    bool is_landscape = IsLandscape();
    if (!is_landscape) {
      LayoutVirtualTouchButtons();
      const int kPadding =
          vtb_pause_->bounds().y + vtb_pause_->bounds().h + 10 * window_scale();
      SDL_Rect canvas_bounds = canvas_->bounds();
      canvas_bounds.y = kPadding;
      canvas_->set_bounds(canvas_bounds);
    }

#if KIWI_IOS
    // In iOS, canvas's dimension is represented as point, not pixel, so it has
    // a smaller scale.
    float canvas_scale = 1.f;
#else
    float canvas_scale = 2.f;
#endif
    if (config_->data().is_stretch_mode) {
      SDL_Rect rect = GetClientBounds();
      if (is_landscape) {
        canvas_scale = static_cast<float>(rect.h) / kDefaultWindowWidth;
      } else {
        canvas_scale = static_cast<float>(rect.w) / kDefaultWindowHeight;
      }
    }
    canvas_->set_frame_scale(canvas_scale);
  }

  HandleResizedEvent();
}

void MainWindow::OnAboutToRenderFrame(Canvas* canvas,
                                      scoped_refptr<NESFrame> frame) {
  const float presentation_scale =
      GetPresentationScale(*frame, canvas->frame_scale());
  const int presentation_width =
      static_cast<int>(frame->width() * presentation_scale);
  const int presentation_height =
      static_cast<int>(frame->height() * presentation_scale);
  SDL_Rect dest_rect;
  if (IsLandscape()) {
    // Always adjusts the canvas to the middle of the render area (excludes menu
    // bar).
    SDL_Rect render_bounds = GetClientBounds();
    dest_rect = {render_bounds.x + (render_bounds.w - presentation_width) / 2,
                 render_bounds.y + (render_bounds.h - presentation_height) / 2,
                 presentation_width, presentation_height};
    canvas->set_bounds(dest_rect);
  } else {
    // Horizontal center align:
    SDL_Rect safe_area_bounds = GetSafeAreaClientBounds();
    dest_rect = {
        safe_area_bounds.x + (safe_area_bounds.w - presentation_width) / 2,
        canvas->bounds().y, presentation_width, presentation_height};

    if (dest_rect.w > safe_area_bounds.w) {
      float s = static_cast<float>(safe_area_bounds.w) / dest_rect.w;
      dest_rect.w *= s;
      dest_rect.h *= s;
      dest_rect.x = safe_area_bounds.x + (safe_area_bounds.w - dest_rect.w) / 2;
    }

    canvas->set_bounds(dest_rect);
  }

  // Updates window size, to fit the frame.
  if (menu_bar_) {
    SDL_Rect menu_rect = menu_bar_->bounds();
    int desired_window_width = dest_rect.w;
    int desired_window_height = menu_rect.h + dest_rect.h;
    Resize(desired_window_width, desired_window_height);
  } else {
    Resize(dest_rect.w, dest_rect.h);
  }
}

void MainWindow::OnInGameSettingsHandleVolume(bool is_left) {
  OnSetAudioVolume(is_left ? 0.f : 1.f);
}

void MainWindow::OnInGameSettingsHandleVolume(const SDL_Rect& volume_bounds,
                                              const SDL_Point& trigger_point) {
  // Shouldn't be here, because in mobile app, there's no volume bar.
  SDL_assert(false);
}

void MainWindow::OnKeyboardMatched() {
  // Matching keyboard, invisible all virtual joystick buttons.
  SetVirtualButtonsVisible(false);
}

void MainWindow::OnJoystickButtonsMatched() {
  // Matching joystick buttons, invisible all virtual joystick buttons.
  SetVirtualButtonsVisible(false);
}

// The window is touched and canvas is appeared, restore all virtual joystick
// buttons.
bool MainWindow::HandleWindowFingerDown() {
#if KIWI_ANDROID
  // TV applications uses joysticks or remote controller.
  if (SDL_IsAndroidTV())
    return false;
#endif

  // Tests one joystick's visibility, to know whether all virtual buttons are
  // visible or not.
  if (canvas_->visible() && vtb_joystick_ && !vtb_joystick_->visible()) {
    SetVirtualButtonsVisible(true);
    return true;
  }

  return false;
}

void MainWindow::StashVirtualButtonsVisible() {
#if KIWI_ANDROID
  // TV applications uses joysticks or remote controller.
  if (SDL_IsAndroidTV())
    return;
#endif

  stashed_virtual_joysticks_visible_state_ =
      vtb_joystick_ && vtb_joystick_->visible();
}

void MainWindow::PopVirtualButtonsVisible() {
#if KIWI_ANDROID
  // TV applications uses joysticks or remote controller.
  if (SDL_IsAndroidTV())
    return;
#endif

  SetVirtualButtonsVisible(stashed_virtual_joysticks_visible_state_);
}

void MainWindow::PauseGameIfDisassemblyVisible() {}
