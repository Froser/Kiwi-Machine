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
#include "utility/math.h"
#include "utility/text_content.h"

constexpr int kSplashDurationMs = 2500;
constexpr float kFadeDurationMs = 1000.f;
constexpr float kClosingDurationMs = 200.f;

constexpr char kHowToPlay[] = R"(How To Play
)";
constexpr char kControllerInstructions[] = R"(
Controller instructions
)";

#if !KIWI_MOBILE

constexpr char kControllerInstructionsContent[] =
    R"(
 Player 1
  UP, DOWN, LEFT, RIGHT: keyboard W, S, A, D
  A, B: keyboard J, K
  SELECT, START: keyboard L, Return

 Player 2
  UP, DOWN, LEFT, RIGHT: keyboard up, down, left, right
  A, B: keyboard Delete, End
  SELECT, START: keyboard PageDown, Home

 Joystick is also available if connected.
 Press L+R to call menu when playing games.
 You may change controller from settings.

)";

#else

constexpr char kControllerInstructionsContent[] =
    R"(
 Joystick is available if connected.
 Press L+R to call menu when playing games.
 You may change controller from settings.

)";

#endif

constexpr char kMenuInstructions[] = R"(
Menu instructions
)";

#if !KIWI_MOBILE

constexpr char kMenuInstructionsContent[] = R"(
You can press UP, DOWN to change groups.
System menu is at the end of the groups.

)";

constexpr char kContinue[] = R"(
Press A or START to continue.)";

#else

constexpr char kMenuInstructionsContent[] = R"(
You can swipe up or down to change groups.
System menu is at the end of the groups.

)";

constexpr char kContinue[] = R"(
Touch screen to continue.)";

#endif

constexpr char kIntroduction[] = R"(Introduction
)";

constexpr char kRetroCollections[] = R"(
Retro Game Collections
)";

#if !KIWI_MOBILE

constexpr char kRetroCollectionsContent[] = R"(
Kiwi machine collects many retro NES games,
and some of them has different versions and
different names and languages, such as
Dragon Quest and Dragon Warrior.

You may press SELECT to choose the version
you want to play.

)";

#else

constexpr char kRetroCollectionsContent[] = R"(
Kiwi machine collects many retro NES games,
and some of them has different versions and
different names and languages, such as
Dragon Quest and Dragon Warrior.

You may touch the version square to choose
the version you want to play.

)";

#endif

constexpr char kSpecialCollections[] = R"(
Special Collections
)";

constexpr char kSpecialCollectionsContent[] = R"(
Kiwi Machine also collected many special ROMs,
such as hacked but popular SMB ROMs, Tank 1990,
etc.

You may press DOWN to switch to the special
rom's group.

)";

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
}

Splash::~Splash() = default;

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
      state_ = SplashState::kHowToPlay;
    }
  } else if (state_ == SplashState::kHowToPlay ||
             state_ == SplashState::kClosingHowToPlay) {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), kSplashSize,
                                                  ImColor(IM_COL32_WHITE));
    int alpha =
        state_ == SplashState::kHowToPlay
            ? Lerp(0, 255,
                   fade_timer_.ElapsedInMilliseconds() / kFadeDurationMs)
            : Lerp(255, 0,
                   fade_timer_.ElapsedInMilliseconds() / kClosingDurationMs);

    TextContent how_to_play_contents(this);
    how_to_play_contents.AddContent(AdjustFont(FontType::kDefault3x),
                                    kHowToPlay);
    how_to_play_contents.AddContent(AdjustFont(FontType::kDefault2x),
                                    kControllerInstructions);
    how_to_play_contents.AddContent(AdjustFont(FontType::kDefault),
                                    kControllerInstructionsContent);
    how_to_play_contents.AddContent(AdjustFont(FontType::kDefault2x),
                                    kMenuInstructions);
    how_to_play_contents.AddContent(AdjustFont(FontType::kDefault),
                                    kMenuInstructionsContent);
    how_to_play_contents.AddContent(AdjustFont(FontType::kDefault), kContinue);

    // Draw all contents
    ImColor font_color(0, 0, 0, alpha);
    how_to_play_contents.DrawContents(font_color);

    if (state_ == SplashState::kClosingHowToPlay && !alpha) {
      fade_timer_.Start();
      state_ = SplashState::kIntroduction;
    }
  } else {
    ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), kSplashSize,
                                                  ImColor(IM_COL32_WHITE));

    int alpha =
        state_ == SplashState::kIntroduction
            ? Lerp(0, 255,
                   fade_timer_.ElapsedInMilliseconds() / kFadeDurationMs)
            : Lerp(255, 0,
                   fade_timer_.ElapsedInMilliseconds() / kClosingDurationMs);

    TextContent introduction(this);
    introduction.AddContent(AdjustFont(FontType::kDefault3x), kIntroduction);
    introduction.AddContent(AdjustFont(FontType::kDefault2x),
                            kRetroCollections);
    introduction.AddContent(AdjustFont(FontType::kDefault),
                            kRetroCollectionsContent);
    introduction.AddContent(AdjustFont(FontType::kDefault2x),
                            kSpecialCollections);
    introduction.AddContent(AdjustFont(FontType::kDefault),
                            kSpecialCollectionsContent);
    introduction.AddContent(AdjustFont(FontType::kDefault), kContinue);

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
    if (state_ == SplashState::kHowToPlay) {
      state_ = SplashState::kClosingHowToPlay;
      fade_timer_.Start();
      PlayEffect(audio_resources::AudioID::kStart);
      return true;
    }

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
