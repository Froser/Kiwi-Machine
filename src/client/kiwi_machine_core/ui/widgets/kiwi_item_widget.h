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

#ifndef UI_WIDGETS_KIWI_ITEM_WIDGET_H_
#define UI_WIDGETS_KIWI_ITEM_WIDGET_H_

#include "ui/widgets/widget.h"

#include <kiwi_nes.h>

#include "utility/fonts.h"

class MainWindow;
class KiwiItemsWidget;
class KiwiItemWidget : public Widget {
 public:
  enum Metrics {
    kItemSelectedWidth = 120,
    kItemSelectedHeight = 140,
    kItemWidth = 90,
    kItemHeight = 105,
    kItemSpacing = 12,
    kItemSizeDecrease = 2,
    // How long it takes to move item from one position to another (ms)
    kItemMoveSpeed = 400,
  };

  explicit KiwiItemWidget(MainWindow* main_window,
                          KiwiItemsWidget* parent,
                          const std::string& title,
                          kiwi::base::RepeatingClosure on_trigger);
  ~KiwiItemWidget() override;

 public:
  void Trigger();
  void OnFingerDown(int x, int y);

  // Sets the cover image data, perhaps jpeg raw data or PNG raw data.
  // Caller must ensure that |cover_img| is never release when KiwiItemWidget is
  // alive.
  void set_cover(const kiwi::nes::Byte* cover_img, size_t cover_size) {
    cover_img_ = cover_img;
    cover_size_ = cover_size;
  }

  void set_selected(bool is_selected) { selected_ = is_selected; }

  void set_sub_items_count(int sub_items_count) {
    sub_items_count_ = sub_items_count;
  }

  // Set current sub item index.
  // -1 means no sub item is selected.
  void set_sub_items_index(int sub_item_index) {
    sub_item_index_ = sub_item_index;
  }

  // Swaps cover, title, and callback.
  void Swap(KiwiItemWidget& rhs);

 protected:
  // Widget:
  void Paint() override;
  bool IsWindowless() override;

 private:
  void CreateTextureIfNotExists();

 private:
  MainWindow* main_window_ = nullptr;
  KiwiItemsWidget* parent_ = nullptr;
  std::string title_;
  const kiwi::nes::Byte* cover_img_ = nullptr;
  size_t cover_size_ = 0u;
  kiwi::base::RepeatingClosure on_trigger_callback_;

  bool selected_ = false;
  int sub_items_count_ = 0;
  int sub_item_index_ = -1;
  SDL_Surface* cover_surface_ = nullptr;
  SDL_Texture* cover_texture_ = nullptr;
  int cover_width_ = 0;
  int cover_height_ = 0;
  SDL_Rect cover_bounds_{0};

  FontType title_font_;
  std::string str_switch_version_;
  FontType font_switch_version_;
};

#endif  // UI_WIDGETS_KIWI_ITEM_WIDGET_H_