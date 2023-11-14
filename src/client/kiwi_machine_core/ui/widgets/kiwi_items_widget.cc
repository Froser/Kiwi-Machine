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
      std::make_unique<KiwiItemWidget>(window(), title, on_trigger);
  item->set_cover(cover_img_ref, cover_size);
  sub_items_[main_item_index].push_back(std::move(item));
}

size_t KiwiItemsWidget::AddItem(const std::string& title,
                                const kiwi::nes::Byte* cover_img_ref,
                                size_t cover_size,
                                kiwi::base::RepeatingClosure on_trigger) {
  std::unique_ptr<KiwiItemWidget> item =
      std::make_unique<KiwiItemWidget>(window(), title, on_trigger);
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

void KiwiItemsWidget::Paint() {
  if (first_paint_) {
    FirstFrame();
  }

  Layout();
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

  items_[current_idx_]->set_sub_items_count(sub_items_[current_idx_].size());
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
    items_[current_idx_]->Trigger();
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kSelect, k)) {
    if (!sub_items_[current_idx_].empty()) {
      PlayEffect(audio_resources::AudioID::kSelect);
      if (sub_item_index_ > -1) {
        // Sub item is currently selected. We have to swap it back first.
        items_[current_idx_]->Swap(*sub_items_[current_idx_][sub_item_index_]);
      }

      // Increase the sub item index, to select the next one.
      ++sub_item_index_;
      if (sub_item_index_ >= sub_items_[current_idx_].size())
        sub_item_index_ = -1;

      if (sub_item_index_ > -1) {
        // Swap the item iff sub item is selected.
        items_[current_idx_]->Swap(*sub_items_[current_idx_][sub_item_index_]);
      }

      items_[current_idx_]->set_sub_items_index(sub_item_index_);
    }
    return true;
  }

  return false;
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