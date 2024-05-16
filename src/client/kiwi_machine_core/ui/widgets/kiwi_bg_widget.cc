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

#include "ui/widgets/kiwi_bg_widget.h"

#include <SDL_image.h>

#include "ui/main_window.h"
#include "ui/styles.h"
#include "ui/window_base.h"
#include "utility/audio_effects.h"
#include "utility/images.h"
#include "utility/key_mapping_util.h"

namespace {
constexpr float kFadeSpeedMs = 100;
}  // namespace

KiwiBgWidget::KiwiBgWidget(MainWindow* main_window, NESRuntimeID runtime_id)
    : Widget(main_window), main_window_(main_window) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  bg_last_render_elapsed_.Start();
}

KiwiBgWidget::~KiwiBgWidget() = default;

void KiwiBgWidget::SetLoading(bool is_loading) {
  is_loading_ = is_loading;
  for (const auto& w : children()) {
    w->set_visible(!is_loading);
  }

  // Activates the timer to draw fade out effect.
  bg_fade_out_timer_.Start();
}

void KiwiBgWidget::Paint() {
  if (is_loading_) {
    float fade_progress =
        (bg_fade_out_timer_.ElapsedInMilliseconds() / kFadeSpeedMs);
    if (fade_progress > 1)
      fade_progress = 1;
    int color = (1 - fade_progress) * 0xff;

    SDL_SetRenderDrawColor(window()->renderer(), color, color, color, 0xff);
    SDL_RenderClear(window()->renderer());
  }
}

bool KiwiBgWidget::IsWindowless() {
  return true;
}
