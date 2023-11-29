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
#include <utility>

#include "ui/main_window.h"
#include "ui/widgets/kiwi_items_widget.h"
#include "utility/fonts.h"
#include "utility/math.h"

KiwiItemWidget::KiwiItemWidget(MainWindow* main_window,
                               KiwiItemsWidget* parent,
                               const std::string& title,
                               kiwi::base::RepeatingClosure on_trigger)
    : Widget(main_window),
      main_window_(main_window),
      parent_(parent),
      title_(title),
      on_trigger_callback_(on_trigger) {}

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

void KiwiItemWidget::OnFingerDown(int x, int y) {
  if (Contains(cover_bounds_, x, y))
    Trigger();
}

void KiwiItemWidget::Swap(KiwiItemWidget& rhs) {
  if (&rhs == this)
    return;

  std::swap(title_, rhs.title_);
  std::swap(cover_img_, rhs.cover_img_);
  std::swap(cover_size_, rhs.cover_size_);
  std::swap(on_trigger_callback_, rhs.on_trigger_callback_);
  std::swap(cover_surface_, rhs.cover_surface_);
  std::swap(cover_texture_, rhs.cover_texture_);
  std::swap(cover_width_, rhs.cover_width_);
  std::swap(cover_height_, rhs.cover_height_);
}

void KiwiItemWidget::Paint() {
  CreateTextureIfNotExists();

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
  cover_bounds_ = {kCoverBound.x + (kCoverBound.w - cover_scaled_width) / 2,
                   kCoverBound.y + (kCoverBound.h - cover_scaled_height) / 2,
                   cover_scaled_width, cover_scaled_height};
  const SDL_Rect& kCoverRect = cover_bounds_;

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  draw_list->AddImage(cover_texture_, ImVec2(kCoverRect.x, kCoverRect.y),
                      ImVec2(kCoverRect.x + cover_scaled_width,
                             kCoverRect.y + cover_scaled_height));

  if (selected_) {
#if !defined(ANDROID)
    constexpr int kSpacingBetweenTitleAndCover = 16;
    ScopedFont scoped_font(FontType::kDefault);
#else
    constexpr int kSpacingBetweenTitleAndCover = 48;
    ScopedFont scoped_font(FontType::kDefault2x);
#endif
    ImFont* font = scoped_font.GetFont();
    {
      ImVec2 title_rect =
          font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, title_.c_str());
      // Move title to the center
      draw_list->AddText(
          font, font->FontSize,
          ImVec2(kBoundsToParent.x + (kBoundsToParent.w - title_rect.x) / 2,
                 kCoverRect.y + cover_scaled_height +
                     kSpacingBetweenTitleAndCover),
          IM_COL32(0, 0, 0, 255), title_.c_str());
    }

    if (sub_items_count_ > 0) {
      // If a game has more than one version, paint the option list to show
      // which version is currently selected.
      constexpr int kSpacingBetweenSubItemPrompt = 10;
#if !defined(ANDROID)
      const int kSubItemPromptSize = 4 * main_window_->window_scale();
#else
      // On mobiles, this area will response finger touch events, so it is a
      // little bit larger.
      const int kSubItemPromptSize = 8 * main_window_->window_scale();
#endif
      int total_item_count = sub_items_count_ + 1;
      const int kPromptWidth =
          kSpacingBetweenSubItemPrompt * (total_item_count - 1) +
          kSubItemPromptSize * total_item_count;
      const int kPromptLeftStart =
          kBoundsToParent.x + (kBoundsToParent.w - kPromptWidth) / 2;
      int prompt_left = kPromptLeftStart;

      // -1 means no sub item is selected. It means the first prompt should be
      // highlighted.
      int current_selected_index = sub_item_index_ + 1;
      for (int i = 0; i < total_item_count; ++i) {
        ImVec2 pos0(prompt_left, kCoverRect.y - kSpacingBetweenTitleAndCover -
                                     kSubItemPromptSize);
        ImVec2 pos1(pos0.x + kSubItemPromptSize, pos0.y + kSubItemPromptSize);
        draw_list->AddRectFilled(pos0, pos1, IM_COL32_BLACK);
        if (current_selected_index == i) {
          draw_list->AddRectFilled(ImVec2(pos0.x + 2, pos0.y + 2),
                                   ImVec2(pos1.x - 2, pos1.y - 2),
                                   IM_COL32(1, 156, 218, 255));
        } else {
          draw_list->AddRectFilled(ImVec2(pos0.x + 2, pos0.y + 2),
                                   ImVec2(pos1.x - 2, pos1.y - 2),
                                   IM_COL32_WHITE);
        }

        // Adding the responding area for switch the game version.
        parent_->AddSubItemTouchArea(
            i, SDL_Rect{static_cast<int>(pos0.x), static_cast<int>(pos0.y),
                        static_cast<int>(pos1.x - pos0.x),
                        static_cast<int>(pos1.y - pos0.y)});
        prompt_left += kSubItemPromptSize + kSpacingBetweenSubItemPrompt;
      }

      // Draw title
      constexpr int kSpacingBetweenTitleAndHint = 13;
#if !defined(ANDROID)
      constexpr char kVersionHintStr[] =
          "(Press select to switch game version)";
#else
      constexpr char kVersionHintStr[] =
          "(Touch the index square to switch game version)";
#endif
      ImVec2 title_rect =
          font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, kVersionHintStr);
      draw_list->AddText(
          font, font->FontSize,
          ImVec2(kBoundsToParent.x + (kBoundsToParent.w - title_rect.x) / 2,
                 kCoverRect.y + cover_scaled_height +
                     kSpacingBetweenTitleAndCover + font->FontSize +
                     kSpacingBetweenTitleAndHint),
          IM_COL32(255, 51, 153, 255), kVersionHintStr);
    }
  }
}

bool KiwiItemWidget::IsWindowless() {
  return true;
}

void KiwiItemWidget::CreateTextureIfNotExists() {
  if (!cover_surface_) {
    SDL_assert(!cover_texture_);
    SDL_RWops* bg_res =
        SDL_RWFromMem(const_cast<unsigned char*>(cover_img_), cover_size_);
    cover_surface_ = IMG_Load_RW(bg_res, 1);
    cover_texture_ =
        SDL_CreateTextureFromSurface(window()->renderer(), cover_surface_);
    SDL_SetTextureScaleMode(cover_texture_, SDL_ScaleModeBest);
    SDL_QueryTexture(cover_texture_, nullptr, nullptr, &cover_width_,
                     &cover_height_);
  }
}