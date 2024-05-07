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

#ifndef UI_WIDGETS_FLEX_ITEMS_WIDGET_H_
#define UI_WIDGETS_FLEX_ITEMS_WIDGET_H_

#include <kiwi_nes.h>

#include "models/nes_runtime.h"
#include "ui/widgets/widget.h"
#include "utility/localization.h"

class MainWindow;
class FlexItemWidget;
class FlexItemsWidget : public Widget {
 public:
  explicit FlexItemsWidget(MainWindow* main_window, NESRuntimeID runtime_id);
  ~FlexItemsWidget() override;

  size_t AddItem(std::unique_ptr<LocalizedStringUpdater> title_updater,
                 const kiwi::nes::Byte* cover_img_ref,
                 size_t cover_size,
                 kiwi::base::RepeatingClosure on_trigger);

  void SetIndex(size_t index);
  bool IsItemSelected(FlexItemWidget* item);

 private:
  void Layout();
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  void TriggerCurrentItem();

  enum Direction {
    kUp,
    kDown,
    kLeft,
    kRight
  };
  size_t FindNextIndex(Direction direction);
  size_t FindNextIndex(int from_index, int to_index);

 protected:
  void Paint() override;
  void OnWindowResized() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) override;

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  std::vector<FlexItemWidget*> items_;
  bool first_paint_ = true;
  size_t current_index_ = 0;
  SDL_Rect current_item_original_bounds_;
};

#endif  // UI_WIDGETS_FLEX_ITEMS_WIDGET_H_