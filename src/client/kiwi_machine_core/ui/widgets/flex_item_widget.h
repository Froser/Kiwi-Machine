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
 private:
  struct Data {
    std::unique_ptr<LocalizedStringUpdater> title_updater;
    const kiwi::nes::Byte* cover_img = nullptr;
    size_t cover_size = 0u;
    kiwi::base::RepeatingClosure on_trigger_callback;
    SDL_Surface* cover_surface = nullptr;
    SDL_Texture* cover_texture = nullptr;
    int cover_width = 0;
    int cover_height = 0;
  };

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
    current_data()->cover_img = cover_img;
    current_data()->cover_size = cover_size;
  }

  void set_row_index(int row_index) { row_index_ = row_index; }
  void set_column_index(int column_index) { column_index_ = column_index; }
  int row_index() { return row_index_; }
  int column_index() { return column_index_; }
  Data* current_data() { return current_data_; }

  SDL_Rect GetSuggestedSize(int item_height);

  void Trigger();

  void AddSubItem(std::unique_ptr<LocalizedStringUpdater> title_updater,
                  const kiwi::nes::Byte* cover_img_ref,
                  size_t cover_size,
                  kiwi::base::RepeatingClosure on_trigger);
  bool has_sub_items() { return sub_data_.size() > 1; }
  bool RestoreToDefaultItem();
  bool SwapToNextSubItem();

 private:
  void CreateTextureIfNotExists();

 protected:
  void Paint() override;

 private:
  MainWindow* main_window_ = nullptr;
  FlexItemsWidget* parent_ = nullptr;
  Data* current_data_ = nullptr;
  SDL_Texture* badge_texture_ = nullptr;

  // Location
  int row_index_ = 0;
  int column_index_ = 0;

  // Fade
  Timer fade_timer_;

  // Children
  std::vector<std::unique_ptr<Data>> sub_data_;
  int current_sub_item_index_ = 0;
};

#endif  // UI_WIDGETS_FLEX_ITEM_WIDGET_H_