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

#include "ui/widgets/toast.h"

#include <imgui.h>

#include "ui/styles.h"
#include "ui/window_base.h"
#include "utility/fonts.h"

namespace {
constexpr int kSpacing = 10;
constexpr int kHeight = 100;
static int g_top = styles::toast::GetTopLeft().x;
}  // namespace

void Toast::ShowToast(WindowBase* window_base,
                      const std::string& message,
                      kiwi::base::TimeDelta duration) {
  Toast* toast = new Toast(window_base, message, duration);
  window_base->AddWidget(std::unique_ptr<Toast>(toast));
}

Toast::Toast(WindowBase* window_base,
             const std::string& message,
             kiwi::base::TimeDelta duration)
    : Widget(window_base), message_(message), duration_(duration) {
  ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
  set_flags(window_flags);
  set_title(message);
  set_bounds(SDL_Rect{g_top, styles::toast::GetTopLeft().y, 200, kHeight});
  g_top += (kHeight + kSpacing);
}

Toast::~Toast() {
  g_top -= (kHeight + kSpacing);
}

void Toast::Paint() {
  if (first_paint_) {
    elapsed_timer_.Start();
    first_paint_ = false;
  }

  if (elapsed_timer_.ElapsedInMilliseconds() > duration_.InMilliseconds()) {
    window()->RemoveWidgetLater(this);
    return;
  }

  ScopedFont font = GetPreferredFont(styles::toast::GetFontSize());
  ImGui::Text("%s", message_.c_str());
}
