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

#ifndef UI_WIDGETS_KIWI_BG_WIDGET_H_
#define UI_WIDGETS_KIWI_BG_WIDGET_H_

#include <SDL.h>
#include <chrono>

#include "ui/widgets/widget.h"
#include "utility/timer.h"

class WindowBase;
class LoadingWidget;
class KiwiBgWidget : public Widget {
 public:
  explicit KiwiBgWidget(WindowBase* window_base);
  ~KiwiBgWidget() override;

 public:
  void SetLoading(bool is_loading);

 protected:
  void Paint() override;
  bool IsWindowless() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;

 private:
  SDL_Texture* bg_texture_ = nullptr;
  int bg_width_ = 0;
  int bg_height_ = 0;
  float bg_offset_even_ = 0.f;
  float bg_offset_odd_ = 0.f;
  bool is_loading_ = false;

  Timer bg_last_render_elapsed_;
  Timer bg_fade_out_timer_;
};

#endif  // UI_WIDGETS_KIWI_BG_WIDGET_H_