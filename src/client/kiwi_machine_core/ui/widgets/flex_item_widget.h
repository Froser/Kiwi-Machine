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
#include <atomic>

#include "ui/widgets/loading_widget.h"
#include "ui/widgets/widget.h"
#include "utility/localization.h"
#include "utility/timer.h"

class MainWindow;
class FlexItemsWidget;
class FlexItemWidget : public Widget {
 public:
  // The parameter 'bool' means whether is trigger action is invoked by finger
  // gesture.
  using TriggerCallback = kiwi::base::RepeatingCallback<void(bool)>;

  using LoadImageCallback = kiwi::base::RepeatingCallback<kiwi::nes::Bytes()>;

 private:
  struct Data {
    std::unique_ptr<LocalizedStringUpdater> title_updater;
    LoadImageCallback image_loader;
    TriggerCallback on_trigger_callback;
    std::atomic<SDL_Texture*> image_texture = nullptr;
    bool requesting_or_requested_texture_ = false;
    int image_width = 0;
    int image_height = 0;
  };

 public:
  explicit FlexItemWidget(MainWindow* main_window,
                          FlexItemsWidget* parent,
                          std::unique_ptr<LocalizedStringUpdater> title_updater,
                          int image_width,
                          int image_height,
                          LoadImageCallback image_loader,
                          TriggerCallback on_trigger);
  ~FlexItemWidget() override;

  // If an item has been filtered, it won't be displayed, and won't participant
  // in layout.
  void set_filtered(bool filtered) { filtered_ = filtered; }
  bool filtered() const { return filtered_; }
  bool MatchFilter(const std::string& filter, int& similarity) const;

  void set_row_index(int row_index) { row_index_ = row_index; }
  void set_column_index(int column_index) { column_index_ = column_index; }
  int row_index() { return row_index_; }
  int column_index() { return column_index_; }
  Data* current_data() { return current_data_; }

  SDL_Rect GetSuggestedSize(int item_height);

  void Trigger(bool triggered_by_finger);

  void AddSubItem(std::unique_ptr<LocalizedStringUpdater> title_updater,
                  int image_width,
                  int image_height,
                  LoadImageCallback image_loader,
                  TriggerCallback on_trigger);
  bool has_sub_items() { return sub_data_.size() > 1; }
  bool RestoreToDefaultItem();
  bool SwapToNextSubItem();

 private:
  void CreateTextureIfNotExists();
  SDL_Texture* LoadImageAndCreateTexture(const kiwi::nes::Bytes& data);

 protected:
  void Paint() override;

 private:
  MainWindow* main_window_ = nullptr;
  FlexItemsWidget* parent_ = nullptr;
  Data* current_data_ = nullptr;
  SDL_Texture* badge_texture_ = nullptr;
  LoadingWidget loading_widget_;

  // Location
  int row_index_ = 0;
  int column_index_ = 0;

  // Fade
  Timer fade_timer_;

  // Children
  std::vector<std::unique_ptr<Data>> sub_data_;
  int current_sub_item_index_ = 0;

  // Filter
  bool filtered_ = false;
};

#endif  // UI_WIDGETS_FLEX_ITEM_WIDGET_H_