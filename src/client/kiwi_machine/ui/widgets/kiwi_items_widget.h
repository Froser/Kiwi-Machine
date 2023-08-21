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

#ifndef UI_WIDGETS_KIWI_ITEMS_WIDGET_H_
#define UI_WIDGETS_KIWI_ITEMS_WIDGET_H_

#include <kiwi_nes.h>
#include <string>

#include "models/nes_runtime.h"
#include "ui/widgets/kiwi_item_widget.h"
#include "ui/widgets/widget.h"
#include "utility/timer.h"

class MainWindow;
class KiwiItemWidget;
// KiwiItemsWidget presents a ROM menu widget.
class KiwiItemsWidget : public Widget {
 public:
  explicit KiwiItemsWidget(MainWindow* main_window, NESRuntimeID runtime_id);
  ~KiwiItemsWidget() override;

 public:
  void AddItem(const std::string& title,
               const kiwi::nes::Byte* cover_img_ref,
               size_t cover_size,
               kiwi::base::RepeatingClosure on_trigger);

 private:
  int GetItemMetrics(KiwiItemWidget::Metrics metrics);
  void CalculateItemsBounds(std::vector<SDL_Rect>& container);
  void Layout();
  void ApplyItemBounds();
  void FirstFrame();
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  void IndexChanged();

 protected:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) override;
  void OnWindowResized() override;

 private:
  MainWindow* main_window_ = nullptr;
  std::vector<KiwiItemWidget*> items_;
  std::vector<SDL_Rect> items_bounds_current_;
  std::vector<SDL_Rect> items_bounds_next_;
  float animation_lerp_ = 0.f;
  Timer animation_counter_;

  bool first_paint_ = true;
  int current_idx_ = 0;
  NESRuntime::Data* runtime_data_ = nullptr;
};

#endif  // UI_WIDGETS_KIWI_ITEMS_WIDGET_H_