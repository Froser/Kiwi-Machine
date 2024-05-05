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

#include "ui/widgets/flex_items_widget.h"

#include <imgui.h>

#include "ui/main_window.h"
#include "ui/widgets/flex_item_widget.h"

namespace {
constexpr int kItemHeightHint = 145;
constexpr int kItemSelectedHighlightedSize = 20;
}  // namespace

FlexItemsWidget::FlexItemsWidget(MainWindow* main_window,
                                 NESRuntimeID runtime_id)
    : Widget(main_window), main_window_(main_window) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  SDL_assert(runtime_data_);
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("KiwiItemsWidget");
}

FlexItemsWidget::~FlexItemsWidget() = default;

size_t FlexItemsWidget::AddItem(
    std::unique_ptr<LocalizedStringUpdater> title_updater,
    const kiwi::nes::Byte* cover_img_ref,
    size_t cover_size,
    kiwi::base::RepeatingClosure on_trigger) {
  std::unique_ptr<FlexItemWidget> item = std::make_unique<FlexItemWidget>(
      main_window_, this, std::move(title_updater), on_trigger);

  item->set_cover(cover_img_ref, cover_size);
  items_.push_back(item.get());
  AddWidget(std::move(item));

  return items_.size() - 1;
}

void FlexItemsWidget::SetIndex(size_t index) {
  current_index_ = index;
  Layout();
}

bool FlexItemsWidget::IsItemSelected(FlexItemWidget* item) {
  SDL_assert(current_index_ < items_.size());
  return items_[current_index_] == item;
}

void FlexItemsWidget::Layout() {
  int anchor_x = 0, anchor_y = 0;
  size_t index = 0;
  for (auto* item : items_) {
    bool is_selected = (index == current_index_);
    SDL_Rect item_bounds = item->GetSuggestedSize(kItemHeightHint, is_selected);
    if (anchor_x + item_bounds.w > bounds().w) {
      anchor_y += item_bounds.h;
      anchor_x = 0;
    }
    item_bounds.x = anchor_x;
    item_bounds.y = anchor_y;

    if (IsItemSelected(item)) {
      item->set_zorder(1);
      if (item_bounds.x == 0) {
        item_bounds.w += kItemSelectedHighlightedSize;
        item_bounds.h += kItemSelectedHighlightedSize;
      } else if (item_bounds.x + item_bounds.w + kItemSelectedHighlightedSize >
                 bounds().w) {
        item_bounds.x -= kItemSelectedHighlightedSize;
        item_bounds.h += kItemSelectedHighlightedSize;
      } else {
        item_bounds.x -= kItemSelectedHighlightedSize;
        item_bounds.w += kItemSelectedHighlightedSize;
        item_bounds.h += kItemSelectedHighlightedSize;
      }
      // TODO consider the item on the bottom
    } else {
      item->set_zorder(0);
    }

    item->set_bounds(item_bounds);
    anchor_x += item_bounds.w;

    index++;
  }
}

void FlexItemsWidget::Paint() {
  if (first_paint_) {
    Layout();
    first_paint_ = false;
  }
}

void FlexItemsWidget::OnWindowResized() {
  Layout();
}