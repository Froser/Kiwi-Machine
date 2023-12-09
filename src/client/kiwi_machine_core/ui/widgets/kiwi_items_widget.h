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
#include <map>
#include <string>
#include <unordered_map>

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
  void AddSubItem(int main_item_index,
                  const std::string& title,
                  const kiwi::nes::Byte* cover_img_ref,
                  size_t cover_size,
                  kiwi::base::RepeatingClosure on_trigger);
  size_t AddItem(const std::string& title,
                 const kiwi::nes::Byte* cover_img_ref,
                 size_t cover_size,
                 kiwi::base::RepeatingClosure on_trigger);
  bool IsEmpty();
  int GetItemCount();
  void SetIndex(int index);
  int current_index() { return current_idx_; }

  void TriggerCurrentItem();
  void SwapCurrentItem();
  // When |sub_item_index| is -1, it means switch it to the original item
  // version.
  void SwapCurrentItemTo(int sub_item_index);
  void AddSubItemTouchArea(int sub_item_index, const SDL_Rect& rect);

 private:
  int GetItemMetrics(KiwiItemWidget::Metrics metrics);
  void CalculateItemsBounds(std::vector<SDL_Rect>& container);
  void Layout();
  void ApplyItemBounds();
  void ApplyItemBoundsByFinger();
  void FirstFrame();
  bool HandleInputEvents(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  int GetNearestIndexByFinger();
  void IndexChanged();
  void ResetSubItemIndex();
  void CleanSubItemTouchAreas();

 protected:
  void Paint() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvents(SDL_ControllerAxisEvent* event) override;
  void OnWindowResized() override;
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;

 private:
  MainWindow* main_window_ = nullptr;
  std::vector<KiwiItemWidget*> items_;
  std::vector<SDL_Rect> items_bounds_current_;
  std::vector<SDL_Rect> items_bounds_next_;
  float animation_lerp_ = 0.f;
  Timer animation_counter_;
  std::map<int, std::vector<std::unique_ptr<KiwiItemWidget>>> sub_items_;
  int sub_item_index_ = -1;  // -1 means do not select sub item.
  std::unordered_map<int, SDL_Rect> sub_items_touch_areas_;

  bool first_paint_ = true;
  int current_idx_ = 0;
  NESRuntime::Data* runtime_data_ = nullptr;

  // Fingers
  bool is_finger_down_ = false;
  bool is_finger_moving_ = false;
  SDL_FingerID finger_id_ = 0;
  float finger_down_x_ = 0;
  float finger_down_y_ = 0;
  float finger_x_ = 0;
  float finger_y_ = 0;
  bool is_moving_horizontally_ = false;
  bool ignore_this_finger_event_ = false;
};

#endif  // UI_WIDGETS_KIWI_ITEMS_WIDGET_H_