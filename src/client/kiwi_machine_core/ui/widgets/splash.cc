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

#include "ui/main_window.h"
#include "ui/widgets/stack_widget.h"

constexpr ImColor kBackgroundColor = ImColor(21, 149, 5);

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

void Splash::Paint() {
  if (first_paint_) {
    timer_.Start();
    first_paint_ = false;
  }

  constexpr float kLogoScaling = .2f;
  SDL_Rect window_bounds = main_window_->GetWindowBounds();
  const ImVec2 kSplashSize(window_bounds.w, window_bounds.h);

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

  // Draws logo
  ImGui::Image(logo, logo_size);

  // Draws background
  ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), kSplashSize,
                                                kBackgroundColor);
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
