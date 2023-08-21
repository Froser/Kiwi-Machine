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

#include "ui/widgets/kiwi_item_widget.h"

#include <SDL_image.h>
#include <imgui.h>

#include "ui/window_base.h"

KiwiItemWidget::KiwiItemWidget(WindowBase* window_base,
                               const std::string& title,
                               kiwi::base::RepeatingClosure on_trigger)
    : Widget(window_base), title_(title), on_trigger_callback_(on_trigger) {}

KiwiItemWidget::~KiwiItemWidget() {
  if (cover_surface_)
    SDL_FreeSurface(cover_surface_);

  if (cover_texture_)
    SDL_DestroyTexture(cover_texture_);
}

void KiwiItemWidget::Trigger() {
  if (on_trigger_callback_)
    on_trigger_callback_.Run();
}

void KiwiItemWidget::Paint() {
  if (first_paint_) {
    SDL_RWops* bg_res =
        SDL_RWFromMem(const_cast<unsigned char*>(cover_img_), cover_size_);
    cover_surface_ = IMG_Load_RW(bg_res, 1);
    cover_texture_ =
        SDL_CreateTextureFromSurface(window()->renderer(), cover_surface_);
    SDL_SetTextureScaleMode(cover_texture_, SDL_ScaleModeBest);
    SDL_QueryTexture(cover_texture_, nullptr, nullptr, &cover_width_,
                     &cover_height_);

    first_paint_ = false;
  }

  // Draws cover and title.
  // Layout is like this:
  // +-------------------+    +-------------------+
  // |                   |    |    ***********    |
  // |   *************   |    |    ***********    |
  // |   *************   |    |    ***********    |
  // |   *************   |    |    ***********    |
  // |   *************   |    |    ***********    |
  // |                   |    |    ***********    |
  // |                   |    |                   |
  // |       Title       |    |       Title       |
  // +-------------------+    +-------------------+

  // The maximum of the cover width takes |kCoverMaxLengthPercentage| of the
  // item's width.
  constexpr float kCoverMaxLengthPercentage = 0.9f;
  const SDL_Rect kBoundsToParent = MapToParent(bounds());
  const SDL_Rect kCoverBound = {
      static_cast<int>(
          kBoundsToParent.x +
          (kBoundsToParent.w * (1 - kCoverMaxLengthPercentage) / 2)),
      kBoundsToParent.y,
      static_cast<int>(kBoundsToParent.w * kCoverMaxLengthPercentage),
      static_cast<int>(kBoundsToParent.w * kCoverMaxLengthPercentage),
  };

  bool is_horizontal = cover_width_ > cover_height_;
  int cover_scaled_width, cover_scaled_height;
  if (is_horizontal) {
    cover_scaled_width =
        cover_width_ > kCoverBound.w ? kCoverBound.w : cover_width_;
    cover_scaled_height = cover_height_ * cover_scaled_width / cover_width_;
  } else {
    cover_scaled_height =
        cover_height_ > kCoverBound.h ? kCoverBound.h : cover_height_;
    cover_scaled_width = cover_width_ * cover_scaled_height / cover_height_;
  }

  // Move cover to the center of the kCoverBound
  SDL_Rect cover_rect = {
      kCoverBound.x + (kCoverBound.w - cover_scaled_width) / 2,
      kCoverBound.y + (kCoverBound.h - cover_scaled_height) / 2,
      cover_scaled_width, cover_scaled_height};

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddImage(cover_texture_, ImVec2(cover_rect.x, cover_rect.y),
                      ImVec2(cover_rect.x + cover_scaled_width,
                             cover_rect.y + cover_scaled_height));

  if (selected_) {
    constexpr int kFontSize = 16;
    constexpr int kSpacingBetweenTitleAndCover = 16;
    ImFont* font = ImGui::GetFont();
    ImVec2 title_rect =
        font->CalcTextSizeA(kFontSize, FLT_MAX, 0.f, title_.c_str());
    // Move title to the center
    draw_list->AddText(
        font, kFontSize,
        ImVec2(
            kBoundsToParent.x + (kBoundsToParent.w - title_rect.x) / 2,
            cover_rect.y + cover_scaled_height + kSpacingBetweenTitleAndCover),
        IM_COL32(0, 0, 0, 255), title_.c_str());
  }
}

bool KiwiItemWidget::IsWindowless() {
  return true;
}
