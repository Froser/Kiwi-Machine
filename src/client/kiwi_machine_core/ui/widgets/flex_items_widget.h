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
  enum class LayoutOption {
    kAdjustScrolling,
    kDoNotAdjustScrolling,
  };

  explicit FlexItemsWidget(MainWindow* main_window, NESRuntimeID runtime_id);
  ~FlexItemsWidget() override;

  size_t AddItem(std::unique_ptr<LocalizedStringUpdater> title_updater,
                 const kiwi::nes::Byte* cover_img_ref,
                 size_t cover_size,
                 kiwi::base::RepeatingClosure on_trigger);
  void AddSubItem(size_t item_index,
                  std::unique_ptr<LocalizedStringUpdater> title_updater,
                  const kiwi::nes::Byte* cover_img_ref,
                  size_t cover_size,
                  kiwi::base::RepeatingClosure on_trigger);

  void SetIndex(size_t index);
  bool IsItemSelected(FlexItemWidget* item);
  void SetActivate(bool activate);
  void ScrollWith(int scrolling_delta, const int* mouse_x, const int* mouse_y);

  bool empty() { return items_.empty(); }
  size_t size() { return items_.size(); }
  size_t current_index() { return current_index_; }
  // Triggers when the widget is about to lose focus.
  void set_back_callback(kiwi::base::RepeatingClosure callback) {
    back_callback_ = callback;
  }

 private:
  void SetIndex(size_t index, LayoutOption option);
  void Layout(LayoutOption option);
  void LayoutAll(LayoutOption);
  void LayoutPartial(LayoutOption);
  // Highlight specified item, calculate its scrolling. If item's bounds
  // exceeded parent's local bounds, it returns true, otherwise it returns
  // false.
  bool HighlightItem(FlexItemWidget* item, LayoutOption option);
  bool HighlightItem(FlexItemWidget* item,
                     LayoutOption option,
                     const SDL_Rect& target_bounds_without_scrolling);
  void ResetAnimationTimers();
  void AdjustBottomRowItemsIfNeeded(LayoutOption option);

  bool HandleInputEvent(SDL_KeyboardEvent* k, SDL_ControllerButtonEvent* c);
  void TriggerCurrentItem();
  void ApplyScrolling(int scrolling);

  enum Direction { kUp, kDown, kLeft, kRight };
  size_t FindNextIndex(Direction direction);
  size_t FindNextIndex(bool down);
  bool FindItemIndexByMousePosition(int x_in_window,
                                    int y_in_window,
                                    size_t& index_out);

  void SwapCurrentItemToNextSubItem();
  void RestoreCurrentItemToDefault();
  void RefreshCurrentItemBounds();

  void PaintDetails();
  enum class MouseOrFingerEventType {
    kMousePressed,
    kMouseReleased,
    kMouseMove,
    kFingerDown,
    kFingerUp,
  };
  enum class MouseButton {
    kLeftButton,
    kRightButton,
    kUnknownButton,
  };
  bool HandleMouseOrFingerEvents(MouseOrFingerEventType type,
                                 MouseButton button,
                                 int x_in_window,
                                 int y_in_window);

 protected:
  void Paint() override;
  void PostPaint() override;
  void OnWindowResized() override;
  bool OnKeyPressed(SDL_KeyboardEvent* event) override;
  bool OnMouseMove(SDL_MouseMotionEvent* event) override;
  bool OnMouseWheel(SDL_MouseWheelEvent* event) override;
  bool OnMousePressed(SDL_MouseButtonEvent* event) override;
  bool OnMouseReleased(SDL_MouseButtonEvent* event) override;
  bool OnControllerButtonPressed(SDL_ControllerButtonEvent* event) override;
  bool OnControllerAxisMotionEvent(SDL_ControllerAxisEvent* event) override;
  void OnWindowPreRender() override;
  void OnWindowPostRender() override;
  int GetHitTestPolicy() override;

#if KIWI_MOBILE
  bool OnTouchFingerDown(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerMove(SDL_TouchFingerEvent* event) override;
  bool OnTouchFingerUp(SDL_TouchFingerEvent* event) override;
#endif

 private:
  MainWindow* main_window_ = nullptr;
  NESRuntime::Data* runtime_data_ = nullptr;
  kiwi::base::RepeatingClosure back_callback_ = kiwi::base::DoNothing();
  std::vector<FlexItemWidget*> items_;
  bool first_paint_ = true;
  size_t current_index_ = 0;
  FlexItemWidget* current_item_widget_ = nullptr;
  SDL_Rect current_item_original_bounds_;
  SDL_Rect current_item_target_bounds_;
  bool need_layout_all_ = true;

  // Scrolling
  int original_view_scrolling_ = 0;
  int target_view_scrolling_ = 0;
  bool updating_view_scrolling_ = false;
  std::map<FlexItemWidget*, SDL_Rect> bounds_map_without_scrolling_;
  bool gesture_locked_ = false;

  bool activate_ = false;
  int rows_ = 0;
  std::unordered_map<int, int> rows_to_first_item_;
  Timer selection_item_timer_;
  Timer scrolling_timer_;

  // Detail widget
  enum { kTop, kBottom } last_detail_widget_position_ = kTop;

  bool scrolling_by_finger_ = false;
#if KIWI_MOBILE
  SDL_TouchFingerEvent last_finger_down_event_;
#endif
};

#endif  // UI_WIDGETS_FLEX_ITEMS_WIDGET_H_