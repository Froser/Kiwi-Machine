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

#include "ui/window_base.h"
#include "utility/images.h"

namespace {
constexpr int kTileSize = 150;
constexpr int kTileAlpha = 64;
constexpr float kPixelPerMs = .05f;  // Tile move distance per millisecond.
constexpr int kPadding = 30;
constexpr float kFadeSpeedMs = 100;
}  // namespace

KiwiBgWidget::KiwiBgWidget(WindowBase* window_base) : Widget(window_base) {
  bg_texture_ =
      GetImage(window()->renderer(), image_resources::ImageID::kBackgroundLogo);
  SDL_SetTextureScaleMode(bg_texture_, SDL_ScaleModeBest);
  SDL_SetTextureAlphaMod(bg_texture_, kTileAlpha);
  SDL_QueryTexture(bg_texture_, nullptr, nullptr, &bg_width_, &bg_height_);
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
  if (!is_loading_) {
    SDL_Rect render_bounds = window()->GetClientBounds();
    int ms = bg_last_render_elapsed_.ElapsedInMillisecondsAndReset();

    bg_offset_even_ -= kPixelPerMs * ms;
    bg_offset_odd_ += kPixelPerMs * ms;
    int offset_even = static_cast<int>(bg_offset_even_) % kTileSize;
    int offset_odd = static_cast<int>(bg_offset_odd_) % kTileSize;

    SDL_SetRenderDrawColor(window()->renderer(), 0xff, 0xff, 0xff, 0xff);
    SDL_RenderClear(window()->renderer());
    bool is_even = true;
    for (int top = render_bounds.y; top < render_bounds.y + render_bounds.h;
         top += kTileSize) {
      for (int left = -kTileSize; left < render_bounds.w + kTileSize;
           left += kTileSize) {
        // Calculate AABB of the tile.
        SDL_Rect aabb_rect = {left + (is_even ? offset_even : offset_odd), top,
                              kTileSize, kTileSize};
        // Calculate padding.
        SDL_Rect dest_rect = {aabb_rect.x + kPadding, aabb_rect.y + kPadding,
                              aabb_rect.w - 2 * kPadding,
                              aabb_rect.h - 2 * kPadding};
        SDL_Rect src_rect = {0, 0, bg_width_, bg_height_};
        SDL_RenderCopy(window()->renderer(), bg_texture_, &src_rect,
                       &dest_rect);
      }
      is_even = !is_even;
    }
  } else {
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

bool KiwiBgWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  // While loading, stop propagate key events to its item widget.
  if (is_loading_)
    return true;

  return Widget::OnKeyPressed(event);
}