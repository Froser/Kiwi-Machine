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

#include <cctype>
#include <tuple>
#include <vector>

#include "build/kiwi_defines.h"
#include "ui/main_window.h"
#include "ui/widgets/stack_widget.h"
#include "utility/audio_effects.h"
#include "utility/images.h"
#include "utility/key_mapping_util.h"
#include "utility/localization.h"
#include "utility/math.h"
#include "utility/richtext_content.h"

constexpr int kSplashDurationMs = 2500;
constexpr float kFadeDurationMs = 1000.f;
constexpr float kClosingDurationMs = 200.f;

Splash::Splash(MainWindow* main_window,
               StackWidget* stack_widget,
               NESRuntimeID runtime_id)
    : Widget(main_window),
      main_window_(main_window),
      stack_widget_(stack_widget) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground;
  set_flags(window_flags);
  set_title("Splash");

  InitializeStrings();
}

Splash::~Splash() = default;

void Splash::InitializeStrings() {
  str_how_to_play_ = GetLocalizedString(string_resources::IDR_HOW_TO_PLAY);
  font_how_to_play_ =
      GetPreferredFontType(PreferredFontSize::k3x, str_how_to_play_.c_str());

#if !KIWI_MOBILE
  str_controller_instructions_ =
      GetLocalizedString(string_resources::IDR_CONTROLLER_INSTRUCTIONS);
  font_controller_instructions_ = GetPreferredFontType(
      PreferredFontSize::k2x, str_controller_instructions_.c_str());

  str_controller_instructions_contents_ = GetLocalizedString(
      string_resources::IDR_CONTROLLER_INSTRUCTIONS_CONTENTS);
  font_controller_instructions_contents_ = GetPreferredFontType(
      PreferredFontSize::k1x, str_controller_instructions_contents_.c_str());

  str_menu_instructions_contents_ =
      GetLocalizedString(string_resources::IDR_MENU_INSTRUCTIONS_CONTENTS);
  font_menu_instructions_contents_ = GetPreferredFontType(
      PreferredFontSize::k1x, str_menu_instructions_contents_.c_str());
#endif

#if !KIWI_MOBILE
  str_continue_ = GetLocalizedString(string_resources::IDR_MENU_CONTINUE);
#else
  str_continue_ =
      GetLocalizedString(string_resources::IDR_MENU_CONTINUE_MOBILE);
#endif
  font_continue_ =
      GetPreferredFontType(PreferredFontSize::k1x, str_continue_.c_str());

  str_introductions_ = GetLocalizedString(string_resources::IDR_INTRODUCTIONS);
  font_introductions_ =
      GetPreferredFontType(PreferredFontSize::k3x, str_introductions_.c_str());

  str_retro_collections_ =
      GetLocalizedString(string_resources::IDR_RETRO_COLLECTIONS);
  font_retro_collections_ = GetPreferredFontType(
      PreferredFontSize::k2x, str_retro_collections_.c_str());

#if !KIWI_MOBILE
  str_retro_collections_contents_ =
      GetLocalizedString(string_resources::IDR_RETRO_COLLECTIONS_CONTENTS);
#else
  str_retro_collections_contents_ = GetLocalizedString(
      string_resources::IDR_RETRO_COLLECTIONS_CONTENTS_MOBILE);
#endif
  font_retro_collections_contents_ = GetPreferredFontType(
      PreferredFontSize::k1x, str_retro_collections_contents_.c_str());

  str_special_collections_ =
      GetLocalizedString(string_resources::IDR_SPECIAL_COLLECTIONS);
  font_special_collections_ = GetPreferredFontType(
      PreferredFontSize::k2x, str_special_collections_.c_str());

  str_special_collections_contents_ =
      GetLocalizedString(string_resources::IDR_SPECIAL_COLLECTIONS_CONTENTS);
  font_special_collections_contents_ = GetPreferredFontType(
      PreferredFontSize::k1x, str_special_collections_contents_.c_str());
}

void Splash::Play() {
  splash_timer_.Start();
  fade_timer_.Start();
  PlayEffect(audio_resources::AudioID::kStartup);
  state_ = SplashState::kLogo;
}

void Splash::Paint() {
  constexpr float kLogoScaling = .2f;
  const ImVec2 kSplashSize(bounds().w, bounds().h);
  auto AdjustFont = [this](FontType font) {
#if !KIWI_MOBILE
    float scale = main_window_->window_scale();
    int adjust = scale < 3.f ? 1 : 0;
    return font == FontType::kDefault
               ? FontType::kDefault
               : (static_cast<FontType>(static_cast<int>(font) - adjust));
#else
    return static_cast<FontType>(static_cast<int>(font) + 1);
#endif
  };

  if (state_ == SplashState::kLogo) {
    SDL_Texture* logo =
        GetImage(window()->renderer(), image_resources::ImageID::kKiwiMachine);
    int w, h;
    SDL_QueryTexture(logo, nullptr, nullptr, &w, &h);

    w *= main_window_->window_scale() * kLogoScaling;
    h *= main_window_->window_scale() * kLogoScaling;

    ImVec2 logo_size(w, h);
    ImVec2 logo_pos((kSplashSize.x - logo_size.x) / 2,
                    (kSplashSize.y - logo_size.y) / 2);

    ImGui::SetCursorPos(logo_pos);
    ImGui::Image(logo, logo_size);

    int current_color =
        Lerp(0, 255, fade_timer_.ElapsedInMilliseconds() / kFadeDurationMs);
    ImColor bg_color(current_color, current_color, current_color);
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), kSplashSize,
                                                  bg_color);

    if (splash_timer_.ElapsedInMilliseconds() > kSplashDurationMs) {
      fade_timer_.Start();
#if !KIWI_MOBILE
      state_ = SplashState::kHowToPlayKeyboard;
#else
      state_ = SplashState::kIntroduction;
#endif
    }
  }
#if !KIWI_MOBILE
  else if (state_ == SplashState::kHowToPlayKeyboard ||
           state_ == SplashState::kClosingHowToPlayKeyboard) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), kSplashSize,
                                                  ImColor(IM_COL32_WHITE));
    int alpha =
        state_ == SplashState::kHowToPlayKeyboard
            ? Lerp(0, 255,
                   fade_timer_.ElapsedInMilliseconds() / kFadeDurationMs)
            : Lerp(255, 0,
                   fade_timer_.ElapsedInMilliseconds() / kClosingDurationMs);

    RichTextContent how_to_play_contents(this);
    how_to_play_contents.AddContent(AdjustFont(font_how_to_play_),
                                    str_how_to_play_.c_str());
    how_to_play_contents.AddContent(AdjustFont(font_controller_instructions_),
                                    str_controller_instructions_.c_str());

    // Calculate controller instruction image's size.
    SDL_Texture* texture = GetImage(
        window()->renderer(), image_resources::ImageID::kControllerInstruction);
    const int kControllerInstructionWidth = .6f * kSplashSize.x;
    int w, h;
    SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
    const int kControllerInstructionHeight =
        static_cast<float>(kControllerInstructionWidth) * h / w;
    how_to_play_contents.AddImage(
        texture,
        ImVec2(kControllerInstructionWidth, kControllerInstructionHeight));

    how_to_play_contents.AddContent(
        AdjustFont(font_menu_instructions_contents_),
        str_menu_instructions_contents_.c_str());
    how_to_play_contents.AddContent(AdjustFont(font_continue_),
                                    str_continue_.c_str());

    // Draw all contents
    ImColor font_color(0, 0, 0, alpha);
    how_to_play_contents.DrawContents(font_color);

    if (state_ == SplashState::kClosingHowToPlayKeyboard && !alpha) {
      fade_timer_.Start();
      state_ = SplashState::kHowToPlayJoystick;
    }
  } else if (state_ == SplashState::kHowToPlayJoystick ||
             state_ == SplashState::kClosingHowToPlayJoystick) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), kSplashSize,
                                                  ImColor(IM_COL32_WHITE));
    int alpha =
        state_ == SplashState::kHowToPlayJoystick
            ? Lerp(0, 255,
                   fade_timer_.ElapsedInMilliseconds() / kFadeDurationMs)
            : Lerp(255, 0,
                   fade_timer_.ElapsedInMilliseconds() / kClosingDurationMs);

    RichTextContent how_to_play_contents(this);
    how_to_play_contents.AddContent(AdjustFont(font_how_to_play_),
                                    str_how_to_play_.c_str());
    how_to_play_contents.AddContent(AdjustFont(font_controller_instructions_),
                                    str_controller_instructions_.c_str());

    // Calculate controller instruction image's size.
    SDL_Texture* texture = GetImage(
        window()->renderer(), image_resources::ImageID::kJoystickInstruction);
    const int kControllerInstructionWidth = .6f * kSplashSize.x;
    int w, h;
    SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);
    const int kControllerInstructionHeight =
        static_cast<float>(kControllerInstructionWidth) * h / w;
    how_to_play_contents.AddImage(
        texture,
        ImVec2(kControllerInstructionWidth, kControllerInstructionHeight));

    how_to_play_contents.AddContent(
        AdjustFont(font_controller_instructions_contents_),
        str_controller_instructions_contents_.c_str());
    how_to_play_contents.AddContent(AdjustFont(font_continue_),
                                    str_continue_.c_str());

    // Draw all contents
    ImColor font_color(0, 0, 0, alpha);
    how_to_play_contents.DrawContents(font_color);

    if (state_ == SplashState::kClosingHowToPlayJoystick && !alpha) {
      fade_timer_.Start();
      state_ = SplashState::kIntroduction;
    }
  }
#endif
  else {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), kSplashSize,
                                                  ImColor(IM_COL32_WHITE));

    int alpha =
        state_ == SplashState::kIntroduction
            ? Lerp(0, 255,
                   fade_timer_.ElapsedInMilliseconds() / kFadeDurationMs)
            : Lerp(255, 0,
                   fade_timer_.ElapsedInMilliseconds() / kClosingDurationMs);

    RichTextContent introduction(this);
    introduction.AddContent(AdjustFont(font_introductions_),
                            str_introductions_.c_str());
    introduction.AddContent(AdjustFont(font_retro_collections_),
                            str_retro_collections_.c_str());
    introduction.AddContent(AdjustFont(font_retro_collections_contents_),
                            str_retro_collections_contents_.c_str());

    introduction.AddContent(AdjustFont(font_special_collections_),
                            str_special_collections_.c_str());
    introduction.AddContent(AdjustFont(font_special_collections_contents_),
                            str_special_collections_contents_.c_str());
    introduction.AddContent(AdjustFont(font_continue_), str_continue_.c_str());

    ImColor font_color(0, 0, 0, alpha);
    introduction.DrawContents(font_color);

    if (state_ == SplashState::kClosing && !alpha)
      stack_widget_->PopWidget();
  }
}

bool Splash::HandleInputEvents(SDL_KeyboardEvent* k,
                               SDL_ControllerButtonEvent* c) {
  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kA, k) ||
      IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kStart, k) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_A) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_START)) {
#if !KIWI_MOBILE
    if (state_ == SplashState::kHowToPlayKeyboard) {
      state_ = SplashState::kClosingHowToPlayKeyboard;
      fade_timer_.Start();
      PlayEffect(audio_resources::AudioID::kStart);
      return true;
    }

    if (state_ == SplashState::kHowToPlayJoystick) {
      state_ = SplashState::kClosingHowToPlayJoystick;
      fade_timer_.Start();
      PlayEffect(audio_resources::AudioID::kStart);
      return true;
    }
#endif

    if (state_ == SplashState::kIntroduction) {
      state_ = SplashState::kClosing;
      fade_timer_.Start();
      PlayEffect(audio_resources::AudioID::kStart);
      return true;
    }
  }
  return false;
}

bool Splash::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvents(event, nullptr);
}

bool Splash::OnControllerButtonPressed(SDL_ControllerButtonEvent* event) {
  return HandleInputEvents(nullptr, event);
}

bool Splash::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  SDL_ControllerButtonEvent c;
  c.button = SDL_CONTROLLER_BUTTON_A;
  return HandleInputEvents(nullptr, &c);
}
