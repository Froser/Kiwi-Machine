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

// KiwiItemWidget presents a ROM label.
class KiwiItemWidget : public Widget {
 public:
  enum Metrics{
    kItemSelectedWidth = 120,
    kItemSelectedHeight = 140,
    kItemWidth = 90,
    kItemHeight = 105,
    kItemSpacing = 12,
    kItemSizeDecrease = 2,
    // How long it takes to move item from one position to another (ms)
    kItemMoveSpeed = 400,
  };

  explicit KiwiItemWidget(WindowBase* window_base,
                          const std::string& title,
                          kiwi::base::RepeatingClosure on_trigger);
  ~KiwiItemWidget() override;

 public:
  void Trigger();

  // Sets the cover image data, perhaps jpeg raw data or PNG raw data.
  // Caller must ensure that |cover_img| is never release when KiwiItemWidget is
  // alive.
  void set_cover(const kiwi::nes::Byte* cover_img, size_t cover_size) {
    cover_img_ = cover_img;
    cover_size_ = cover_size;
  }

  void set_selected(bool is_selected) { selected_ = is_selected; }

 protected:
  // Widget:
  void Paint() override;
  bool IsWindowless() override;

 private:
  std::string title_;
  const kiwi::nes::Byte* cover_img_ = nullptr;
  size_t cover_size_ = 0u;
  kiwi::base::RepeatingClosure on_trigger_callback_;
  bool first_paint_ = true;

  bool selected_ = false;
  SDL_Surface* cover_surface_ = nullptr;
  SDL_Texture* cover_texture_ = nullptr;
  int cover_width_ = 0;
  int cover_height_ = 0;
};

#endif  // UI_WIDGETS_KIWI_ITEM_WIDGET_H_