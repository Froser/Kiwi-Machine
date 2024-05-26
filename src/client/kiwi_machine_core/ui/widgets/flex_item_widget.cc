// Copyright (C) 2024 Yisi Yu
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

#include "ui/widgets/flex_item_widget.h"

#include <SDL_image.h>
#include <imgui.h>

#include "ui/main_window.h"
#include "ui/widgets/flex_items_widget.h"

namespace {
constexpr float kCoverHWRatio = 250.f / 200;
constexpr float kFadeDurationInMs = 1000;
}  // namespace

FlexItemWidget::FlexItemWidget(
    MainWindow* main_window,
    FlexItemsWidget* parent,
    std::unique_ptr<LocalizedStringUpdater> title_updater,
    kiwi::base::RepeatingClosure on_trigger)
    : Widget(main_window),
      main_window_(main_window),
      parent_(parent),
      title_updater_(std::move(title_updater)),
      on_trigger_callback_(on_trigger) {
  SDL_assert(parent_);

  // Since title won't change during the instance created, calculates the font
  // once to improve performance.
  SDL_assert(title_updater_);
  // UpdateTitleAndFont();
}

FlexItemWidget::~FlexItemWidget() {
  if (cover_surface_)
    SDL_FreeSurface(cover_surface_);

  if (cover_texture_)
    SDL_DestroyTexture(cover_texture_);
}

void FlexItemWidget::CreateTextureIfNotExists() {
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

SDL_Rect FlexItemWidget::GetSuggestedSize(int item_height, bool is_selected) {
  SDL_Rect bs = bounds();
  bs.h = item_height;
  CreateTextureIfNotExists();
  bs.w = bs.h * cover_width_ / cover_height_;
  return bs;
}

void FlexItemWidget::Trigger() {
  if (on_trigger_callback_)
    on_trigger_callback_.Run();
}

void FlexItemWidget::Paint() {
  CreateTextureIfNotExists();
  const SDL_Rect kBoundsToParent = MapToGlobal(bounds());
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw stretched image
  draw_list->AddImage(cover_texture_,
                      ImVec2(kBoundsToParent.x, kBoundsToParent.y),
                      ImVec2(kBoundsToParent.x + kBoundsToParent.w,
                             kBoundsToParent.y + kBoundsToParent.h));

  // Highlight selected item.
  if (parent_->IsItemSelected(this)) {
    int elapsed = fade_timer_.ElapsedInMilliseconds();
    int rgb = static_cast<int>(512 * elapsed / kFadeDurationInMs) % 512;
    if (rgb > 255)
      rgb = 512 - rgb;

    draw_list->AddRect(ImVec2(kBoundsToParent.x, kBoundsToParent.y),
                       ImVec2(kBoundsToParent.x + kBoundsToParent.w,
                              kBoundsToParent.y + kBoundsToParent.h),
                       ImColor(rgb, rgb, rgb));
  } else {
    fade_timer_.Reset();
  }
}
