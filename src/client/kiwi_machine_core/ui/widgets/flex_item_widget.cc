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
    : Widget(main_window), main_window_(main_window), parent_(parent) {
  SDL_assert(parent_);

  // Initialize the default data
  std::unique_ptr<Data> default_data = std::make_unique<Data>();
  default_data->title_updater = std::move(title_updater);
  default_data->on_trigger_callback = std::move(on_trigger);
  current_data_ = default_data.get();
  sub_data_.push_back(std::move(default_data));

  // Since title won't change during the instance created, calculates the font
  // once to improve performance.
  SDL_assert(current_data()->title_updater);
}

FlexItemWidget::~FlexItemWidget() {
  if (current_data()->cover_surface)
    SDL_FreeSurface(current_data()->cover_surface);

  if (current_data()->cover_texture)
    SDL_DestroyTexture(current_data()->cover_texture);
}

void FlexItemWidget::CreateTextureIfNotExists() {
  if (!current_data()->cover_surface) {
    SDL_assert(!current_data()->cover_texture);
    SDL_RWops* bg_res =
        SDL_RWFromMem(const_cast<unsigned char*>(current_data()->cover_img),
                      current_data()->cover_size);
    current_data()->cover_surface = IMG_Load_RW(bg_res, 1);
    current_data()->cover_texture = SDL_CreateTextureFromSurface(
        window()->renderer(), current_data()->cover_surface);
    SDL_SetTextureScaleMode(current_data()->cover_texture, SDL_ScaleModeBest);
    SDL_QueryTexture(current_data()->cover_texture, nullptr, nullptr,
                     &current_data()->cover_width,
                     &current_data()->cover_height);
  }
}

SDL_Rect FlexItemWidget::GetSuggestedSize(int item_height, bool is_selected) {
  SDL_Rect bs = bounds();
  bs.h = item_height;
  CreateTextureIfNotExists();
  bs.w = bs.h * current_data()->cover_width / current_data()->cover_height;
  return bs;
}

void FlexItemWidget::Trigger() {
  if (current_data()->on_trigger_callback)
    current_data()->on_trigger_callback.Run();
}

void FlexItemWidget::AddSubItem(
    std::unique_ptr<LocalizedStringUpdater> title_updater,
    const kiwi::nes::Byte* cover_img_ref,
    size_t cover_size,
    kiwi::base::RepeatingClosure on_trigger) {
  std::unique_ptr<Data> data = std::make_unique<Data>();
  data->title_updater = std::move(title_updater);
  data->cover_img = cover_img_ref;
  data->cover_size = cover_size;
  data->on_trigger_callback = on_trigger;
  sub_data_.push_back(std::move(data));
}

void FlexItemWidget::SwapToNextSubItem() {
  ++current_sub_item_index;
  if (current_sub_item_index >= sub_data_.size())
    current_sub_item_index = 0;

  current_data_ = sub_data_[current_sub_item_index].get();
}

void FlexItemWidget::Paint() {
  CreateTextureIfNotExists();
  const SDL_Rect kBoundsToWindow = MapToWindow(bounds());
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw stretched image
  draw_list->AddImage(current_data()->cover_texture,
                      ImVec2(kBoundsToWindow.x, kBoundsToWindow.y),
                      ImVec2(kBoundsToWindow.x + kBoundsToWindow.w,
                             kBoundsToWindow.y + kBoundsToWindow.h));

  // Highlight selected item.
  if (parent_->IsItemSelected(this)) {
    int elapsed = fade_timer_.ElapsedInMilliseconds();
    int rgb = static_cast<int>(512 * elapsed / kFadeDurationInMs) % 512;
    if (rgb > 255)
      rgb = 512 - rgb;

    draw_list->AddRect(ImVec2(kBoundsToWindow.x, kBoundsToWindow.y),
                       ImVec2(kBoundsToWindow.x + kBoundsToWindow.w,
                              kBoundsToWindow.y + kBoundsToWindow.h),
                       ImColor(rgb, rgb, rgb));
  } else {
    fade_timer_.Reset();
  }
}
