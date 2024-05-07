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

#ifndef UI_WIDGETS_FLEX_ITEM_WIDGET_H_
#define UI_WIDGETS_FLEX_ITEM_WIDGET_H_

#include <kiwi_nes.h>

#include "ui/widgets/widget.h"
#include "utility/localization.h"
#include "utility/timer.h"

class MainWindow;
class FlexItemsWidget;
class FlexItemWidget : public Widget {
 public:
  explicit FlexItemWidget(MainWindow* main_window,
                          FlexItemsWidget* parent,
                          std::unique_ptr<LocalizedStringUpdater> title_updater,
                          kiwi::base::RepeatingClosure on_trigger);
  ~FlexItemWidget() override;

  // Sets the cover image data, perhaps jpeg raw data or PNG raw data.
  // Caller must ensure that |cover_img| is never release when KiwiItemWidget is
  // alive.
  void set_cover(const kiwi::nes::Byte* cover_img, size_t cover_size) {
    cover_img_ = cover_img;
    cover_size_ = cover_size;
  }

  SDL_Rect GetSuggestedSize(int item_height, bool is_selected);

  void Trigger();

 private:
  void CreateTextureIfNotExists();

 protected:
  void Paint() override;

 private:
  MainWindow* main_window_ = nullptr;
  FlexItemsWidget* parent_ = nullptr;
  std::string title_;
  std::unique_ptr<LocalizedStringUpdater> title_updater_;
  const kiwi::nes::Byte* cover_img_ = nullptr;
  size_t cover_size_ = 0u;
  kiwi::base::RepeatingClosure on_trigger_callback_;
  SDL_Surface* cover_surface_ = nullptr;
  SDL_Texture* cover_texture_ = nullptr;
  int cover_width_ = 0;
  int cover_height_ = 0;

  // Fade
  Timer fade_timer_;
};

#endif  // UI_WIDGETS_FLEX_ITEM_WIDGET_H_