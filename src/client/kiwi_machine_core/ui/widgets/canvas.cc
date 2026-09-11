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

#include "ui/widgets/canvas.h"
#include "ui/widgets/canvas_observer.h"
#include "ui/window_base.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>

#include "resources/string_resources.h"
#include "utility/fonts.h"
#include "utility/localization.h"

namespace {
constexpr int kBrightThreshold = 220;
constexpr int kHDTextureHintDurationMs = 3000;
constexpr int kHDTextureHintFadeInMs = 220;
constexpr int kHDTextureHintFadeOutMs = 360;
constexpr float kHDTextureHintMaximumWidthRatio = .92f;
constexpr float kHDTextureHintBadgeWidth = 52.f;
constexpr float kHDTextureHintBadgeHeight = 24.f;

bool IsColorBrightEnough(int r, int g, int b) {
  float luminance = 0.299 * r + 0.587 * g + 0.114 * b;
  return luminance > kBrightThreshold;
}

ImU32 WithAlpha(ImU32 color, float alpha) {
  return (color & 0x00ffffff) |
         (static_cast<ImU32>(std::clamp(alpha, 0.f, 1.f) * 255.f) << 24);
}

}  // namespace

Canvas::Canvas(WindowBase* window_base, NESRuntimeID runtime_id)
    : Widget(window_base) {
  frame_ = kiwi::base::MakeRefCounted<NESFrame>(window_base, runtime_id);
  frame_->AddObserver(this);
  set_bounds(SDL_Rect{0, 0, kNESFrameDefaultWidth, kNESFrameDefaultHeight});
}

Canvas::~Canvas() = default;

void Canvas::Clear() {
  SDL_RenderClear(window()->renderer());
}

int Canvas::GetZapperState() {
  using State = kiwi::nes::IODevices::InputDevice::ZapperState;
  int state = State::kNone;
  if (mouse_or_finger_down_) {
    state |= State::kTriggered;
  }

  if (ZapperTest(Input::kMouse) || ZapperTest(Input::kFinger))
    state |= State::kLightSensed;

  return state;
}

void Canvas::AddObserver(CanvasObserver* observer) {
  observers_.insert(observer);
}

void Canvas::RemoveObserver(CanvasObserver* observer) {
  observers_.erase(observer);
}

void Canvas::Paint() {
  // Notify all observers
  for (CanvasObserver* observer : observers_) {
    observer->OnAboutToRenderFrame(this, frame_.get());
  }

  SDL_Rect src_rect = {0, 0, frame_->width(), frame_->height()};
  SDL_Rect dest_rect = bounds();
  SDL_RenderCopy(window()->renderer(), frame_->texture(), &src_rect,
                 &dest_rect);
  PaintHDTextureToggleHint();
}

bool Canvas::IsWindowless() {
  return true;
}

bool Canvas::OnKeyPressed(SDL_KeyboardEvent* event) {
  last_hd_toggle_input_was_controller_ = false;
  if (hd_texture_toggle_available_ && event->keysym.sym == SDLK_TAB &&
      event->repeat == 0) {
    InvokeHDTextureToggle();
    return true;
  }
#if !KIWI_WASM
  if (event->keysym.sym == SDLK_ESCAPE) {
    InvokeInGameMenu();
    return true;
  }
  return false;
#else
  // WASM uses web ui menu, instead of the in-game menu.
  return false;
#endif
}

bool Canvas::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  last_hd_toggle_input_was_controller_ = true;
  SDL_GameController* controller =
      SDL_GameControllerFromInstanceID(event->which);
  const bool left_shoulder = SDL_GameControllerGetButton(
      controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
  const bool right_shoulder = SDL_GameControllerGetButton(
      controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
  if (event->button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
    right_shoulder_pending_ = true;
  }
  if (left_shoulder && right_shoulder) {
    shoulder_chord_used_ = true;
    InvokeInGameMenu();
    return true;
  }
  return hd_texture_toggle_available_ &&
         event->button == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
}

bool Canvas::OnControllerButtonReleased(SDL_ControllerButtonEvent* event) {
  if (event->button != SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) {
    return false;
  }

  const bool should_toggle = hd_texture_toggle_available_ &&
                             right_shoulder_pending_ && !shoulder_chord_used_;
  right_shoulder_pending_ = false;
  shoulder_chord_used_ = false;
  if (should_toggle) {
    last_hd_toggle_input_was_controller_ = true;
    InvokeHDTextureToggle();
  }
  return hd_texture_toggle_available_;
}

bool Canvas::OnMousePressed(SDL_MouseButtonEvent* event) {
  last_hd_toggle_input_was_controller_ = false;
  mouse_or_finger_down_ = true;
  return Widget::OnMousePressed(event);
}

bool Canvas::OnMouseReleased(SDL_MouseButtonEvent* event) {
  mouse_or_finger_down_ = false;
  return Widget::OnMouseReleased(event);
}

bool Canvas::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  mouse_or_finger_down_ = true;

  SDL_Rect bounds = window()->GetClientBounds();
  touch_point_ = std::make_pair(event->x * bounds.w, event->y * bounds.h);
  return Widget::OnTouchFingerDown(event);
}

bool Canvas::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  mouse_or_finger_down_ = false;
  touch_point_ = std::nullopt;
  return Widget::OnTouchFingerUp(event);
}

void Canvas::OnShouldRender(int since_last_frame_ms) {}

void Canvas::InvokeInGameMenu() {
  if (on_menu_trigger_)
    on_menu_trigger_.Run();
}

void Canvas::SetHDTextureToggleAvailable(bool available) {
  hd_texture_toggle_available_ = available;
  hd_texture_hint_visible_ = available;
  right_shoulder_pending_ = false;
  shoulder_chord_used_ = false;
  if (available) {
    hd_texture_hint_timer_.Reset();
    hd_texture_hint_badge_ = HDEditionBadge();
  }
}

void Canvas::InvokeHDTextureToggle() {
  if (on_hd_texture_toggle_) {
    on_hd_texture_toggle_.Run();
  }
}

void Canvas::PaintHDTextureToggleHint() {
  if (!hd_texture_toggle_available_ || !hd_texture_hint_visible_) {
    return;
  }

  const int elapsed_ms = hd_texture_hint_timer_.ElapsedInMilliseconds();
  if (elapsed_ms >= kHDTextureHintDurationMs) {
    hd_texture_hint_visible_ = false;
    return;
  }

  float alpha = 1.f;
  if (elapsed_ms < kHDTextureHintFadeInMs) {
    alpha = static_cast<float>(elapsed_ms) / kHDTextureHintFadeInMs;
  } else if (elapsed_ms > kHDTextureHintDurationMs - kHDTextureHintFadeOutMs) {
    alpha = static_cast<float>(kHDTextureHintDurationMs - elapsed_ms) /
            kHDTextureHintFadeOutMs;
  }

  const int string_id =
      last_hd_toggle_input_was_controller_
          ? string_resources::IDR_HD_TEXTURE_TOGGLE_GAMEPAD_HINT
          : string_resources::IDR_HD_TEXTURE_TOGGLE_KEYBOARD_HINT;
  const std::string& text = GetLocalizedString(string_id);
  ScopedFont preferred_font = GetPreferredFont(
      PreferredFontSize::k1x, text.c_str(), FontType::kSystemDefault);
  ImFont* font = preferred_font.GetFont();
  float font_size = preferred_font.GetFontSize();
  const SDL_Rect canvas_bounds = MapToWindow(bounds());
  const float maximum_width = canvas_bounds.w * kHDTextureHintMaximumWidthRatio;
  float horizontal_padding = std::max(12.f, font_size * .8f);
  float vertical_padding = std::max(8.f, font_size * .45f);
  float content_spacing = std::max(8.f, font_size * .55f);
  const float fixed_width =
      horizontal_padding * 2.f + kHDTextureHintBadgeWidth + content_spacing;
  const float maximum_text_width = std::max(1.f, maximum_width - fixed_width);
  ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, text.c_str());
  if (text_size.x > maximum_text_width) {
    font_size = std::max(12.f, font_size * maximum_text_width / text_size.x);
    text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.f, text.c_str());
    horizontal_padding = std::max(12.f, font_size * .8f);
    vertical_padding = std::max(8.f, font_size * .45f);
    content_spacing = std::max(8.f, font_size * .55f);
  }

  const float panel_width = horizontal_padding * 2.f +
                            kHDTextureHintBadgeWidth + content_spacing +
                            text_size.x;
  const float panel_height =
      std::max(kHDTextureHintBadgeHeight, text_size.y) + vertical_padding * 2.f;
  const ImVec2 box_min(canvas_bounds.x + (canvas_bounds.w - panel_width) * .5f,
                       canvas_bounds.y + canvas_bounds.h * .055f);
  const ImVec2 box_max(box_min.x + panel_width, box_min.y + panel_height);

  ImDrawList* draw_list = ImGui::GetForegroundDrawList();
  const float rounding = std::max(6.f, panel_height * .12f);
  draw_list->AddRectFilled(box_min, box_max,
                           WithAlpha(IM_COL32(7, 14, 20, 255), alpha * .82f),
                           rounding);
  draw_list->AddRect(box_min, box_max,
                     WithAlpha(IM_COL32(19, 218, 255, 255), alpha * .9f),
                     rounding, 0, std::max(1.f, font_size * .07f));

  const SDL_Rect badge_bounds = {
      static_cast<int>(box_min.x + horizontal_padding),
      static_cast<int>(box_min.y +
                       (panel_height - kHDTextureHintBadgeHeight) * .5f),
      static_cast<int>(kHDTextureHintBadgeWidth),
      static_cast<int>(kHDTextureHintBadgeHeight),
  };
  hd_texture_hint_badge_.Paint(draw_list, badge_bounds, true, alpha, false,
                               HDEditionBadge::Presentation::kStandalone);

  const float text_left = box_min.x + horizontal_padding +
                          kHDTextureHintBadgeWidth + content_spacing;
  draw_list->AddText(
      font, font_size,
      ImVec2(text_left, box_min.y + (panel_height - text_size.y) * .5f),
      WithAlpha(IM_COL32(255, 255, 255, 255), alpha), text.c_str());
}

Canvas::ZapperDetails Canvas::CreateZapperDetailsByMouseOrFingerPosition(
    int x,
    int y) {
  // bounds here is relative to client rect, which will contain menu bar's
  // height when debug is enabled.
  int non_client_height = window()->GetClientBounds().y;
  SDL_Rect adjusted_bounds = bounds();
  adjusted_bounds.y -= non_client_height;

  SDL_Rect bounds_to_window = MapToWindow(adjusted_bounds);
  int relative_x = x - bounds_to_window.x;
  int relative_y = y - bounds_to_window.y;

  return ZapperDetails{
      relative_x * kNESFrameDefaultWidth / bounds_to_window.w,
      relative_y * kNESFrameDefaultHeight / bounds_to_window.h};
}

bool Canvas::ZapperTest(Input input) {
  ZapperDetails details;
  if (input == Input::kMouse) {
    int x, y;
    SDL_GetMouseState(&x, &y);
    details = CreateZapperDetailsByMouseOrFingerPosition(x, y);
  } else {
    // By gesture
    if (touch_point_) {
      details = CreateZapperDetailsByMouseOrFingerPosition(
          std::get<0>(*touch_point_), std::get<1>(*touch_point_));
    } else {
      return false;
    }
  }

  if (details.original_x < 0 || details.original_x >= kNESFrameDefaultWidth ||
      details.original_y < 0 || details.original_y >= kNESFrameDefaultHeight) {
    return false;
  }

  const size_t data_index =
      kNESFrameDefaultWidth * details.original_y + details.original_x;
  kiwi::nes::Color color = frame_->GetCurrentFrame()[data_index];
  return IsColorBrightEnough(color & 0xff, (color >> 8) & 0xff,
                             (color >> 16) & 0xff);
}
