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

#include "ui/widgets/kiwi_items_widget.h"

#include <imgui.h>

#include "ui/main_window.h"
#include "ui/widgets/kiwi_item_widget.h"
#include "utility/audio_effects.h"
#include "utility/key_mapping_util.h"
#include "utility/math.h"

KiwiItemsWidget::KiwiItemsWidget(MainWindow* main_window,
                                 NESRuntimeID runtime_id)
    : Widget(main_window), main_window_(main_window) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  SDL_assert(runtime_data_);
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("KiwiItemsWidget");
}

KiwiItemsWidget::~KiwiItemsWidget() = default;

void KiwiItemsWidget::AddSubItem(int main_item_index,
                                 const std::string& title,
                                 const kiwi::nes::Byte* cover_img_ref,
                                 size_t cover_size,
                                 kiwi::base::RepeatingClosure on_trigger) {
  std::unique_ptr<KiwiItemWidget> item =
      std::make_unique<KiwiItemWidget>(main_window_, this, title, on_trigger);
  item->set_cover(cover_img_ref, cover_size);
  sub_items_[main_item_index].push_back(std::move(item));
}

size_t KiwiItemsWidget::AddItem(const std::string& title,
                                const kiwi::nes::Byte* cover_img_ref,
                                size_t cover_size,
                                kiwi::base::RepeatingClosure on_trigger) {
  std::unique_ptr<KiwiItemWidget> item =
      std::make_unique<KiwiItemWidget>(main_window_, this, title, on_trigger);
  item->set_cover(cover_img_ref, cover_size);
  items_.push_back(item.get());
  AddWidget(std::move(item));

  items_bounds_current_.resize(items_.size());
  items_bounds_next_.resize(items_.size());
  return items_.size() - 1;
}

bool KiwiItemsWidget::IsEmpty() {
  return items_.empty();
}

int KiwiItemsWidget::GetItemCount() {
  return items_.size();
}

void KiwiItemsWidget::SetIndex(int index) {
  ResetSubItemIndex();
  current_idx_ = index;
  IndexChanged();
}

void KiwiItemsWidget::TriggerCurrentItem() {
  items_[current_idx_]->Trigger();
}

void KiwiItemsWidget::SwapCurrentItem() {
  SwapCurrentItemTo(sub_item_index_ + 1);
}

void KiwiItemsWidget::SwapCurrentItemTo(int sub_item_index) {
  if (sub_item_index_ > -1) {
    // Sub item is currently selected. We have to swap it back first.
    items_[current_idx_]->Swap(*sub_items_[current_idx_][sub_item_index_]);
  }

  // Increase the sub item index, to select the next one.
  if (sub_item_index >= sub_items_[current_idx_].size())
    sub_item_index = -1;

  if (sub_item_index > -1) {
    // Swap the item iff sub item is selected.
    items_[current_idx_]->Swap(*sub_items_[current_idx_][sub_item_index]);
  }

  items_[current_idx_]->set_sub_items_index(sub_item_index);
  sub_item_index_ = sub_item_index;
}

void KiwiItemsWidget::Paint() {
  if (first_paint_) {
    FirstFrame();
  }

  CleanSubItemTouchAreas();

  if (is_finger_moving_ && is_moving_horizontally_) {
    if (Contains(bounds(), finger_x_, finger_y_)) {
      // Do not update animation counter (by just reset it).
      animation_counter_.ElapsedInMillisecondsAndReset();
      ApplyItemBoundsByFinger();
    }
  } else {
    Layout();
  }
}

bool KiwiItemsWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvents(event, nullptr);
}

bool KiwiItemsWidget::OnControllerButtonPressed(
    SDL_ControllerButtonEvent* event) {
  return HandleInputEvents(nullptr, event);
}

bool KiwiItemsWidget::OnControllerAxisMotionEvents(
    SDL_ControllerAxisEvent* event) {
  return HandleInputEvents(nullptr, nullptr);
}

void KiwiItemsWidget::OnWindowResized() {
  set_bounds(SDL_Rect{bounds().x, bounds().y, window()->GetClientBounds().w,
                      window()->GetClientBounds().h});

  // Recalculate bounds because window size is changed.
  CalculateItemsBounds(items_bounds_current_);
  items_bounds_next_ = items_bounds_current_;
  ApplyItemBounds();

  Widget::OnWindowResized();
}
bool KiwiItemsWidget::OnTouchFingerDown(SDL_TouchFingerEvent* event) {
  if (!is_finger_down_) {
    is_finger_down_ = true;
    finger_id_ = event->fingerId;
    finger_x_ = finger_down_x_ = event->x;
    finger_y_ = finger_down_y_ = event->y;
    is_moving_horizontally_ = false;
    ignore_this_finger_event_ = false;

    // Checks if inside the sub item's area
    SDL_Rect client_rect = window()->GetClientBounds();
    int x = event->x * client_rect.w, y = event->y * client_rect.h;
    for (const auto& sub_item : sub_items_touch_areas_) {
      if (Contains(sub_item.second, x, y)) {
        // Parameter |sub_item_index| in SwapCurrentItemTo() starts from -1.
        // -1 means the original version, and 0 means the first alternative
        // version in sub item's list.
        SwapCurrentItemTo(sub_item.first - 1);
        // When the event processes sub item's swapping, it won't process any
        // other thing.
        ignore_this_finger_event_ = true;
        break;
      }
    }
  }

  return false;
}

bool KiwiItemsWidget::OnTouchFingerUp(SDL_TouchFingerEvent* event) {
  if (is_finger_down_ && event->fingerId == finger_id_) {
    if (!ignore_this_finger_event_) {
      if (is_moving_horizontally_) {
        // When moving horizontally:
        SetIndex(GetNearestIndexByFinger());
        for (size_t i = 0; i < children().size(); ++i) {
          items_bounds_next_[i] = children()[i]->bounds();
        }
        IndexChanged();
      } else {
        // When clicking, only if finger is not moving will trigger the item.
        for (size_t i = 0; i < children().size(); ++i) {
          SDL_Rect client_rect = window()->GetClientBounds();
          SDL_Rect child_bounds = children()[i]->bounds();
          int x = finger_down_x_ * client_rect.w,
              y = finger_down_y_ * client_rect.h;
          if (Contains(child_bounds, x, y)) {
            if (i == current_idx_) {
              items_[current_idx_]->OnFingerDown(x, y);
            } else {
              SetIndex(i);
            }
            break;
          }
        }
      }
    }

    is_finger_down_ = false;
    is_finger_moving_ = false;
    is_moving_horizontally_ = false;
  }
  return false;
}

bool KiwiItemsWidget::OnTouchFingerMove(SDL_TouchFingerEvent* event) {
  if (ignore_this_finger_event_)
    return true;

  if (is_finger_down_ && event->fingerId == finger_id_) {
    finger_x_ = event->x;
    finger_y_ = event->y;

    if (!is_finger_moving_) {
      SDL_Rect rect = window()->GetClientBounds();
      is_moving_horizontally_ =
          std::abs((finger_y_ - finger_down_y_) * rect.h) <
          std::abs((finger_x_ - finger_down_x_) * rect.w);
      is_finger_moving_ = true;
    }

    // Propagates the event to the next widget.
    if (!is_moving_horizontally_)
      return false;
  }
  return true;
}

int KiwiItemsWidget::GetItemMetrics(KiwiItemWidget::Metrics metrics) {
  SDL_assert(main_window_ && main_window_ == window());
  return static_cast<int>(metrics) * main_window_->window_scale();
}

void KiwiItemsWidget::CalculateItemsBounds(std::vector<SDL_Rect>& container) {
  SDL_assert(container.size() == items_.size());

  // Caches each child's next bounds.
  // Items are layout like this:
  //
  //           +------+
  //   +---+   |      |   +---+
  //   |   |   |      |   |   |
  //   |   | S |      | S |   |
  //   +---+   |      |   +---+
  //           +------+
  // Where  S is spacing

  // Draw selected item in the middle
  SDL_Rect rect_center{
      (bounds().w - GetItemMetrics(KiwiItemWidget::kItemSelectedWidth)) / 2,
      (bounds().h - GetItemMetrics(KiwiItemWidget::kItemSelectedHeight)) / 2,
      GetItemMetrics(KiwiItemWidget::kItemSelectedWidth),
      GetItemMetrics(KiwiItemWidget::kItemSelectedHeight)};
  container[current_idx_] = rect_center;

  // Calculates left
  {
    int left = rect_center.x;
    int top = (bounds().h - GetItemMetrics(KiwiItemWidget::kItemHeight)) / 2;
    int height = GetItemMetrics(KiwiItemWidget::kItemHeight);
    for (int i = current_idx_ - 1; i >= 0; --i) {
      left -= GetItemMetrics(KiwiItemWidget::kItemSpacing) +
              GetItemMetrics(KiwiItemWidget::kItemWidth);
      container[i] = SDL_Rect{
          left, top, GetItemMetrics(KiwiItemWidget::kItemWidth), height};
      top += GetItemMetrics(KiwiItemWidget::kItemSizeDecrease);
      height -= GetItemMetrics(KiwiItemWidget::kItemSizeDecrease) * 2;
    }
  }

  // Calculates right
  {
    int left = rect_center.x + rect_center.w +
               GetItemMetrics(KiwiItemWidget::kItemSpacing);
    int top = (bounds().h - GetItemMetrics(KiwiItemWidget::kItemHeight)) / 2;
    int height = GetItemMetrics(KiwiItemWidget::kItemHeight);
    for (size_t i = current_idx_ + 1; i < items_.size(); ++i) {
      container[i] = SDL_Rect{
          left, top, GetItemMetrics(KiwiItemWidget::kItemWidth), height};
      left += GetItemMetrics(KiwiItemWidget::kItemSpacing) +
              GetItemMetrics(KiwiItemWidget::kItemWidth);
      top += GetItemMetrics(KiwiItemWidget::kItemSizeDecrease);
      height -= GetItemMetrics(KiwiItemWidget::kItemSizeDecrease) * 2;
    }
  }
}

void KiwiItemsWidget::Layout() {
  // Calculate animation lerp factor.
  float elapsed = animation_counter_.ElapsedInMillisecondsAndReset();
  if (animation_lerp_ >= 1.f) {
    items_bounds_current_ = items_bounds_next_;
    animation_lerp_ = 1.f;
    return;
  }

  animation_lerp_ +=
      elapsed / (KiwiItemWidget::kItemMoveSpeed / main_window_->window_scale());
  if (animation_lerp_ >= 1.f)
    animation_lerp_ = 1.f;

  ApplyItemBounds();
}

void KiwiItemsWidget::ApplyItemBounds() {
  for (size_t i = 0; i < items_.size(); ++i) {
    items_[i]->set_selected(i == current_idx_);
    items_[i]->set_bounds(
        Lerp(items_bounds_current_[i], items_bounds_next_[i], animation_lerp_));
  }

  size_t sub_items_count = sub_items_[current_idx_].size();
  items_[current_idx_]->set_sub_items_count(sub_items_count);
}

void KiwiItemsWidget::ApplyItemBoundsByFinger() {
  SDL_assert(is_finger_moving_);
  SDL_Rect rect = window()->GetClientBounds();
  int dx = (finger_x_ - finger_down_x_) * rect.w;

  for (size_t i = 0; i < children().size(); ++i) {
    children()[i]->set_bounds(
        SDL_Rect{items_bounds_current_[i].x + dx, items_bounds_current_[i].y,
                 items_bounds_current_[i].w, items_bounds_current_[i].h});
  }
}

void KiwiItemsWidget::FirstFrame() {
  // Calculates current bounds in first frame.
  animation_lerp_ = 0.f;
  CalculateItemsBounds(items_bounds_current_);
  items_bounds_next_ = items_bounds_current_;
  ApplyItemBounds();
  animation_counter_.Start();
  first_paint_ = false;
}

bool KiwiItemsWidget::HandleInputEvents(SDL_KeyboardEvent* k,
                                        SDL_ControllerButtonEvent* c) {
  if (!is_finger_down_) {
    if (IsKeyboardOrControllerAxisMotionMatch(
            runtime_data_, kiwi::nes::ControllerButton::kLeft, k) ||
        c && c->button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
      if (current_idx_ > 0) {
        PlayEffect(audio_resources::AudioID::kSelect);
        SetIndex(current_idx_ - 1);
      }
      return true;
    }

    if (IsKeyboardOrControllerAxisMotionMatch(
            runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
        c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
      if (current_idx_ < items_.size() - 1) {
        PlayEffect(audio_resources::AudioID::kSelect);
        SetIndex(current_idx_ + 1);
      }
      return true;
    }

    if (IsKeyboardOrControllerAxisMotionMatch(
            runtime_data_, kiwi::nes::ControllerButton::kStart, k) ||
        c && c->button == SDL_CONTROLLER_BUTTON_START ||
        IsKeyboardOrControllerAxisMotionMatch(
            runtime_data_, kiwi::nes::ControllerButton::kA, k) ||
        c && c->button == SDL_CONTROLLER_BUTTON_A) {
      PlayEffect(audio_resources::AudioID::kStart);
      TriggerCurrentItem();
      return true;
    }

    if (IsKeyboardOrControllerAxisMotionMatch(
            runtime_data_, kiwi::nes::ControllerButton::kSelect, k)) {
      if (!sub_items_[current_idx_].empty()) {
        PlayEffect(audio_resources::AudioID::kSelect);
        SwapCurrentItem();
      }
      return true;
    }
  }

  return false;
}

int KiwiItemsWidget::GetNearestIndexByFinger() {
  SDL_assert(!children().empty());
  if (children().size() == 1)
    return 0;

  const int kCenter = bounds().w / 2;
  int t = std::abs(children()[0]->bounds().x - kCenter);

  for (size_t i = 1; i < children().size(); ++i) {
    int dis = std::abs(children()[i]->bounds().x - kCenter);
    if (dis < t)
      t = dis;
    else
      return i - 1;
  }

  return children().size() - 1;
}

void KiwiItemsWidget::IndexChanged() {
  animation_lerp_ = .0f;
  items_bounds_current_ = items_bounds_next_;
  CalculateItemsBounds(items_bounds_next_);
}

void KiwiItemsWidget::ResetSubItemIndex() {
  if (sub_item_index_ == -1)
    return;

  int last_sub_item_index = sub_item_index_;
  sub_item_index_ = -1;
  items_[current_idx_]->set_sub_items_index(sub_item_index_);
  if (!sub_items_[current_idx_].empty()) {
    // Swaps back. Reset the current item.
    items_[current_idx_]->Swap(*sub_items_[current_idx_][last_sub_item_index]);
  }
}

void KiwiItemsWidget::AddSubItemTouchArea(int sub_item_index,
                                          const SDL_Rect& rect) {
  sub_items_touch_areas_[sub_item_index] = rect;
}

void KiwiItemsWidget::CleanSubItemTouchAreas() {
  sub_items_touch_areas_.clear();
}