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

#include "ui/widgets/flex_items_widget.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <set>

#include "ui/application.h"
#include "ui/main_window.h"
#include "ui/styles.h"
#include "ui/widgets/filter_widget.h"
#include "utility/algorithm.h"
#include "utility/audio_effects.h"
#include "utility/key_mapping_util.h"
#include "utility/math.h"
#include "utility/zip_reader.h"

namespace {
const int kItemHeightHint = styles::flex_items_widget::GetItemHeightHint();
const int kItemSelectedHighlightedSize =
    styles::flex_items_widget::GetItemHighlightedSize();
constexpr int kItemAnimationMs = 50;
constexpr int kScrollingAnimationMs = 20;
#if KIWI_ANDROID
constexpr int kDetailWidgetMargin = 48;
constexpr int kDetailWidgetPadding = 16;
constexpr int kDetailWidgetLineSpacing = 8;
constexpr int kDetailWidgetAccentWidth = 8;
constexpr float kDetailWidgetCornerRadius = 10.f;
constexpr float kDetailWidgetMaxWidthRatio = .8f;
#elif KIWI_IOS
constexpr int kDetailWidgetMargin = 20;
constexpr int kDetailWidgetPadding = 8;
constexpr int kDetailWidgetLineSpacing = 4;
constexpr int kDetailWidgetAccentWidth = 4;
constexpr float kDetailWidgetCornerRadius = 6.f;
constexpr float kDetailWidgetMaxWidthRatio = .75f;
#else
constexpr int kDetailWidgetMargin = 25;
constexpr int kDetailWidgetPadding = 10;
constexpr int kDetailWidgetLineSpacing = 4;
constexpr int kDetailWidgetAccentWidth = 4;
constexpr float kDetailWidgetCornerRadius = 6.f;
constexpr float kDetailWidgetMaxWidthRatio = .55f;
#endif
constexpr int kFilterWidgetMargin = kDetailWidgetMargin;
constexpr int kFilterWidgetPadding = kDetailWidgetPadding;
constexpr int kItemHoverDurationMs = 1000;
constexpr int kTextureCacheMarginRows = 2;
constexpr float kMinimumFlingVelocity = 120.f;
constexpr float kInertialDeceleration = 4.5f;
constexpr int kMaximumInertialFrameMs = 50;
constexpr ImU32 kLibraryBackgroundColor = IM_COL32(244, 252, 227, 255);
constexpr ImU32 kBackgroundLimeColor = IM_COL32(116, 184, 22, 34);
constexpr ImU32 kBackgroundSkyColor = IM_COL32(56, 174, 201, 28);
constexpr ImU32 kBackgroundCoralColor = IM_COL32(230, 125, 100, 24);
constexpr ImU32 kDetailBackgroundColor = IM_COL32(33, 42, 47, 238);
constexpr ImU32 kDetailBorderColor = IM_COL32(72, 85, 79, 255);
constexpr ImU32 kDetailAccentColor = IM_COL32(130, 201, 30, 255);
constexpr ImU32 kDetailMetaColor = IM_COL32(192, 235, 117, 255);
constexpr ImU32 kDetailTextColor = IM_COL32(248, 249, 250, 255);
constexpr float kTwoPi = 6.28318530718f;

enum class BackgroundShape {
  kCircle,
  kPlus,
  kDiamond,
  kPixels,
};

struct BackgroundParticle {
  BackgroundShape shape;
  float x_ratio;
  float phase;
  float duration_seconds;
  float size_ratio;
  ImU32 color;
};

constexpr BackgroundParticle kBackgroundParticles[] = {
    {BackgroundShape::kCircle, .06f, .12f, 17.f, .012f, kBackgroundLimeColor},
    {BackgroundShape::kPlus, .19f, .68f, 21.f, .016f, kBackgroundSkyColor},
    {BackgroundShape::kDiamond, .33f, .34f, 19.f, .014f, kBackgroundCoralColor},
    {BackgroundShape::kPixels, .47f, .86f, 24.f, .013f, kBackgroundLimeColor},
    {BackgroundShape::kPlus, .61f, .47f, 18.f, .012f, kBackgroundCoralColor},
    {BackgroundShape::kCircle, .74f, .05f, 22.f, .017f, kBackgroundSkyColor},
    {BackgroundShape::kPixels, .87f, .58f, 20.f, .014f, kBackgroundLimeColor},
    {BackgroundShape::kDiamond, .96f, .27f, 16.f, .011f, kBackgroundSkyColor},
    {BackgroundShape::kCircle, .13f, .79f, 23.f, .018f, kBackgroundCoralColor},
    {BackgroundShape::kDiamond, .27f, .03f, 18.f, .010f, kBackgroundLimeColor},
    {BackgroundShape::kPixels, .39f, .55f, 20.f, .016f, kBackgroundSkyColor},
    {BackgroundShape::kCircle, .54f, .22f, 15.f, .011f, kBackgroundLimeColor},
    {BackgroundShape::kPlus, .68f, .91f, 25.f, .015f, kBackgroundCoralColor},
    {BackgroundShape::kDiamond, .79f, .41f, 19.f, .013f, kBackgroundSkyColor},
    {BackgroundShape::kPlus, .91f, .73f, 22.f, .018f, kBackgroundLimeColor},
    {BackgroundShape::kPixels, .99f, .15f, 17.f, .010f, kBackgroundCoralColor},
};

#if KIWI_MOBILE
constexpr size_t kBackgroundParticleCount = 8;
constexpr float kBackgroundMaximumShapeRadius = 18.f;
constexpr float kBackgroundMaximumDrift = 6.f;
#else
constexpr size_t kBackgroundParticleCount = 16;
constexpr float kBackgroundMaximumShapeRadius = 24.f;
constexpr float kBackgroundMaximumDrift = 14.f;
#endif

#if BUILDFLAG(IS_MAC)
constexpr float kWheelVelocitySmoothing = 0.35f;
constexpr float kMaximumWheelFlingVelocity = 6000.f;
constexpr Uint32 kWheelVelocitySampleTimeoutMs = 100;
#endif

using FilterSearchIndex = std::vector<std::vector<std::string>>;

struct AutoReset {
  AutoReset(bool& value) : value_(value) {}
  ~AutoReset() { value_ = false; }

  bool& value_;
};

int CalculateIntersectionArea(const SDL_Rect& lhs, const SDL_Rect& rhs) {
  SDL_assert(lhs.h == rhs.h);
  int lhs_x2 = lhs.x + lhs.w;
  int rhs_x2 = rhs.x + rhs.w;

  return std::min(lhs_x2, rhs_x2) - std::max(lhs.x, rhs.x);
}

void DrawBackgroundShape(ImDrawList* draw_list,
                         BackgroundShape shape,
                         const ImVec2& center,
                         float radius,
                         float angle,
                         ImU32 color,
                         float thickness) {
  const ImVec2 axis_x(std::cos(angle) * radius, std::sin(angle) * radius);
  const ImVec2 axis_y(-std::sin(angle) * radius, std::cos(angle) * radius);
  switch (shape) {
    case BackgroundShape::kCircle:
      draw_list->AddCircle(center, radius, color, 24, thickness);
      break;
    case BackgroundShape::kPlus:
      draw_list->AddLine(ImVec2(center.x - axis_x.x, center.y - axis_x.y),
                         ImVec2(center.x + axis_x.x, center.y + axis_x.y),
                         color, thickness);
      draw_list->AddLine(ImVec2(center.x - axis_y.x, center.y - axis_y.y),
                         ImVec2(center.x + axis_y.x, center.y + axis_y.y),
                         color, thickness);
      break;
    case BackgroundShape::kDiamond: {
      const ImVec2 points[] = {
          ImVec2(center.x + axis_y.x, center.y + axis_y.y),
          ImVec2(center.x + axis_x.x, center.y + axis_x.y),
          ImVec2(center.x - axis_y.x, center.y - axis_y.y),
          ImVec2(center.x - axis_x.x, center.y - axis_x.y),
      };
      draw_list->AddPolyline(points, 4, color, ImDrawFlags_Closed, thickness);
      break;
    }
    case BackgroundShape::kPixels: {
      const float pixel_size = radius * .42f;
      const float offset = radius * .58f;
      for (int y = -1; y <= 1; y += 2) {
        for (int x = -1; x <= 1; x += 2) {
          const ImVec2 pixel_center(center.x + axis_x.x * x * offset / radius +
                                        axis_y.x * y * offset / radius,
                                    center.y + axis_x.y * x * offset / radius +
                                        axis_y.y * y * offset / radius);
          draw_list->AddRectFilled(ImVec2(pixel_center.x - pixel_size * .5f,
                                          pixel_center.y - pixel_size * .5f),
                                   ImVec2(pixel_center.x + pixel_size * .5f,
                                          pixel_center.y + pixel_size * .5f),
                                   color);
        }
      }
      break;
    }
  }
}

void DrawLibraryBackground(ImDrawList* draw_list,
                           const SDL_Rect& bounds,
                           float elapsed_seconds) {
  const ImVec2 bounds_min(bounds.x, bounds.y);
  const ImVec2 bounds_max(bounds.x + bounds.w, bounds.y + bounds.h);
  draw_list->AddRectFilled(bounds_min, bounds_max, kLibraryBackgroundColor);

  if (bounds.w <= 0 || bounds.h <= 0)
    return;

  const float minimum_extent = static_cast<float>(std::min(bounds.w, bounds.h));
  const float line_thickness = std::clamp(minimum_extent / 800.f, 1.f, 2.f);
  const float drift =
      std::min(static_cast<float>(bounds.w) * .008f, kBackgroundMaximumDrift);

  draw_list->PushClipRect(bounds_min, bounds_max, true);
  for (size_t i = 0; i < kBackgroundParticleCount; ++i) {
    const BackgroundParticle& particle = kBackgroundParticles[i];
    const float progress = std::fmod(
        elapsed_seconds / particle.duration_seconds + particle.phase, 1.f);
    const float radius = std::clamp(minimum_extent * particle.size_ratio, 7.f,
                                    kBackgroundMaximumShapeRadius);
    const float angle =
        elapsed_seconds * kTwoPi / (particle.duration_seconds * 1.8f) +
        particle.phase * kTwoPi;
    const float x =
        bounds.x + bounds.w * particle.x_ratio +
        std::sin(elapsed_seconds * kTwoPi / particle.duration_seconds +
                 particle.phase * kTwoPi) *
            drift;
    const float y = bounds.y - radius + progress * (bounds.h + radius * 2.f);
    DrawBackgroundShape(draw_list, particle.shape, ImVec2(x, y), radius, angle,
                        particle.color, line_thickness);
  }
  draw_list->PopClipRect();
}

std::vector<size_t> CalculateFilteredResultOnIOThread(
    std::shared_ptr<const FilterSearchIndex> search_index,
    std::shared_ptr<std::atomic<uint64_t>> current_request_id,
    uint64_t request_id,
    const std::string& filter) {
  std::vector<std::pair<size_t, int>> matches;
  for (size_t item_index = 0; item_index < search_index->size(); ++item_index) {
    if (current_request_id->load(std::memory_order_relaxed) != request_id)
      return {};

    for (const std::string& candidate : (*search_index)[item_index]) {
      if (HasString(candidate, filter)) {
        const int similarity = static_cast<int>(candidate.size()) -
                               static_cast<int>(filter.size());
        matches.emplace_back(item_index, similarity);
        break;
      }
    }
  }

  std::sort(
      matches.begin(), matches.end(),
      [](const auto& lhs, const auto& rhs) { return lhs.second < rhs.second; });

  std::vector<size_t> result;
  result.reserve(matches.size());
  for (const auto& match : matches)
    result.push_back(match.first);
  return result;
}

}  // namespace

FlexItemsWidget::FlexItemsWidget(MainWindow* main_window,
                                 NESRuntimeID runtime_id)
    : Widget(main_window), main_window_(main_window) {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  SDL_assert(runtime_data_);
  set_flags(ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs);
  set_title("KiwiItemsWidget");

  std::unique_ptr<FilterWidget> filter_widget = std::make_unique<FilterWidget>(
      main_window_, kiwi::base::BindRepeating(&FlexItemsWidget::OnFilter,
                                              kiwi::base::Unretained(this)));
  filter_widget->set_visible(false);
  filter_widget_ = filter_widget.get();
  AddWidget(std::move(filter_widget));
}

FlexItemsWidget::~FlexItemsWidget() {
  filter_request_id_->fetch_add(1, std::memory_order_relaxed);
  filter_lifetime_token_.reset();
}

size_t FlexItemsWidget::AddItem(
    std::unique_ptr<LocalizedStringUpdater> title_updater,
    int image_width,
    int image_height,
    FlexItemWidget::LoadImageCallback image_loader,
    FlexItemWidget::TriggerCallback on_trigger) {
  if (image_width && image_height) {
    std::unique_ptr<FlexItemWidget> item = std::make_unique<FlexItemWidget>(
        main_window_, this, std::move(title_updater), image_width, image_height,
        image_loader, on_trigger);
    std::vector<std::string> filter_strings = item->GetFilterStrings();
    items_.push_back(item.get());
    all_items_.push_back(item.get());
    AddWidget(std::move(item));
    EnsureUniqueFilterSearchIndex();
    filter_search_index_->push_back(std::move(filter_strings));
    need_layout_all_ = true;
  } else {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Boxart width or height is zero. Won't added to widget: %s",
                title_updater->GetLocalizedString().c_str());
  }
  return items_.size() - 1;
}

void FlexItemsWidget::AddSubItem(
    size_t item_index,
    std::unique_ptr<LocalizedStringUpdater> title_updater,
    int image_width,
    int image_height,
    FlexItemWidget::LoadImageCallback image_loader,
    FlexItemWidget::TriggerCallback on_trigger) {
  SDL_assert(item_index < items_.size());
  items_[item_index]->AddSubItem(std::move(title_updater), image_width,
                                 image_height, image_loader, on_trigger);
  EnsureUniqueFilterSearchIndex();
  (*filter_search_index_)[item_index] = items_[item_index]->GetFilterStrings();
}

void FlexItemsWidget::SetIndex(size_t index) {
  SetIndex(index, LayoutOption::kAdjustScrolling, false);
}

void FlexItemsWidget::SetIndex(size_t index, LayoutOption option, bool force) {
  if (current_index_ != index || force) {
    RestoreCurrentItemToDefault();
    current_index_ = index;
    if (items_.empty())
      current_index_ = 0;
    else if (current_index_ >= items_.size())
      current_index_ = items_.size() - 1;
    Layout(option);
  }
}

bool FlexItemsWidget::IsItemSelected(FlexItemWidget* item) {
  SDL_assert(current_index_ < items_.size());
  return items_[current_index_] == item;
}

void FlexItemsWidget::SetActivate(bool activate) {
  if (activate_ != activate) {
    activate_ = activate;
    Layout(LayoutOption::kDoNotAdjustScrolling);
  }
  if (!activate_) {
    StopInertialScrolling();
    wheel_scroll_remainder_ = 0.f;
    wheel_scroll_velocity_ = 0.f;
    last_wheel_scroll_delta_ = 0.f;
    last_wheel_motion_timestamp_ = 0;
    has_wheel_velocity_sample_ = false;
    wheel_gesture_active_ = false;
#if KIWI_MOBILE
    touch_active_ = false;
    scrolling_by_finger_ = false;
    gesture_locked_ = false;
#endif
    filter_widget_->EndFilter();
  }
}

void FlexItemsWidget::ScrollWith(int scrolling_delta,
                                 const int* mouse_x,
                                 const int* mouse_y) {
  if (items_.empty())
    return;

  // Alternative rom's cover may have a different size, so we restore here.
  RestoreCurrentItemToDefault();

  // Scrolling
  target_view_scrolling_ += scrolling_delta;

  // Avoid exceeding the top of the widget
  if (target_view_scrolling_ >= 0)
    target_view_scrolling_ = 0;

  // Avoid exceeding the bottom of the widget
  FlexItemWidget* last_item = items_[items_.size() - 1];
  SDL_Rect last_item_absolute_bounds = bounds_map_without_scrolling_[last_item];
  if (last_item_absolute_bounds.y + last_item_absolute_bounds.h +
          target_view_scrolling_ <
      bounds().h) {
    target_view_scrolling_ =
        bounds().h - last_item_absolute_bounds.h - last_item_absolute_bounds.y;
  }

  FlexItemWidget* first_item = items_[0];
  SDL_Rect first_item_absolute_bounds =
      bounds_map_without_scrolling_[first_item];
  if (first_item_absolute_bounds.y + target_view_scrolling_ > 0) {
    target_view_scrolling_ = first_item_absolute_bounds.y;
  }

  updating_view_scrolling_ = true;

  // Highlight
  if (mouse_x && mouse_y) {
    size_t item_index;
    bool current_index_exceeded_bottom;
    if (FindItemIndexByMousePosition(*mouse_x, *mouse_y, item_index)) {
      current_index_ = item_index;
      current_index_exceeded_bottom = HighlightItem(
          items_[item_index], LayoutOption::kDoNotAdjustScrolling);
    } else {
      current_index_exceeded_bottom = HighlightItem(
          current_item_widget_, LayoutOption::kDoNotAdjustScrolling);
    }
    if (current_index_exceeded_bottom)
      AdjustBottomRowItemsIfNeeded(LayoutOption::kDoNotAdjustScrolling);
  } else {
    bool current_index_exceeded_bottom = HighlightItem(
        current_item_widget_, LayoutOption::kDoNotAdjustScrolling);
    if (current_index_exceeded_bottom)
      AdjustBottomRowItemsIfNeeded(LayoutOption::kDoNotAdjustScrolling);
  }
}

void FlexItemsWidget::StartInertialScrolling(float velocity) {
  if (std::abs(velocity) < kMinimumFlingVelocity) {
    StopInertialScrolling();
    return;
  }

  inertial_scrolling_ = true;
  inertial_scroll_velocity_ = velocity;
  inertial_scroll_remainder_ = 0.f;
  inertial_scroll_timer_.Reset();
}

void FlexItemsWidget::StopInertialScrolling() {
  inertial_scrolling_ = false;
  inertial_scroll_velocity_ = 0.f;
  inertial_scroll_remainder_ = 0.f;
}

void FlexItemsWidget::UpdateInertialScrolling() {
  if (!inertial_scrolling_)
    return;

  const int elapsed_ms =
      std::min(inertial_scroll_timer_.ElapsedInMillisecondsAndReset(),
               kMaximumInertialFrameMs);
  if (elapsed_ms <= 0)
    return;

  const float elapsed_seconds = elapsed_ms / 1000.f;
  const float scroll_delta =
      inertial_scroll_velocity_ * elapsed_seconds + inertial_scroll_remainder_;
  const int pixel_delta = static_cast<int>(scroll_delta);
  inertial_scroll_remainder_ = scroll_delta - pixel_delta;

  if (pixel_delta != 0) {
    const int previous_scrolling = target_view_scrolling_;
    ScrollWith(pixel_delta, nullptr, nullptr);
    if (target_view_scrolling_ == previous_scrolling) {
      StopInertialScrolling();
      return;
    }
  }

  inertial_scroll_velocity_ *=
      std::exp(-kInertialDeceleration * elapsed_seconds);
  if (std::abs(inertial_scroll_velocity_) < kMinimumFlingVelocity)
    StopInertialScrolling();
}

void FlexItemsWidget::ShowFilterWidget() {
  filter_widget_->set_bounds(GetLocalBounds());
  filter_widget_->BeginFilter();
}

void FlexItemsWidget::Layout(LayoutOption option) {
  if (need_layout_all_)
    LayoutAll(option);
  else
    LayoutPartial(option);
}

void FlexItemsWidget::LayoutAll(LayoutOption option) {
  const SDL_Rect kLocalBounds = GetLocalBounds();
  if (SDL_RectEmpty(&kLocalBounds))
    return;

  original_view_scrolling_ = target_view_scrolling_;
  int anchor_x = 0, anchor_y = 0;
  int column_index = 0, row_index = 0;
  size_t index = 0;
  rows_to_first_item_.clear();
  rows_to_first_item_[0] = 0;
  bounds_map_without_scrolling_.clear();

  bool current_index_exceeded_bottom = false;
  for (auto* item : items_) {
    SDL_Rect item_bounds = item->GetSuggestedSize(kItemHeightHint);
    if (anchor_x + item_bounds.w > bounds().w) {
      anchor_y += item_bounds.h;
      anchor_x = 0;
      row_index++;
      column_index = 0;
      rows_to_first_item_[row_index] = index;
    } else {
      column_index++;
    }

    item->set_row_index(row_index);
    item->set_column_index(column_index);

    item_bounds.x = anchor_x;
    item_bounds.y = anchor_y;
    anchor_x += item_bounds.w;

    bounds_map_without_scrolling_[item] = item_bounds;
    if (IsItemSelected(item)) {
      current_index_exceeded_bottom = HighlightItem(item, option);
    }

    if (target_view_scrolling_ == 0) {
      // Applying scrolling above won't set visibility when view_scrolling_ is
      // zero. So we set it here.
      item->set_visible(SDL_HasIntersection(&item_bounds, &kLocalBounds));
    }

    index++;
  }

  // Updates max row index:
  rows_ = row_index;

  // If the current index is the last row, viewport need to be adjusted.
  if (current_index_exceeded_bottom)
    AdjustBottomRowItemsIfNeeded(option);

  ResetAnimationTimers();
  need_layout_all_ = false;
}

void FlexItemsWidget::LayoutPartial(LayoutOption option) {
  if (items_.empty())
    return;

  if (current_item_widget_ != items_[current_index_]) {
    original_view_scrolling_ = target_view_scrolling_;
    bool current_index_exceeded_bottom =
        HighlightItem(items_[current_index_], option);
    // If the current index is the last row, viewport need to be adjusted.
    if (current_index_exceeded_bottom)
      AdjustBottomRowItemsIfNeeded(option);

    ResetAnimationTimers();
  }
}

bool FlexItemsWidget::HighlightItem(FlexItemWidget* item, LayoutOption option) {
  return HighlightItem(item, option, bounds_map_without_scrolling_[item]);
}

bool FlexItemsWidget::HighlightItem(
    FlexItemWidget* item,
    LayoutOption option,
    const SDL_Rect& target_bounds_without_scrolling) {
  bool current_index_exceeded_bottom = false;
  current_item_widget_ = item;

  SDL_Rect item_target_bounds = target_bounds_without_scrolling;
  current_item_original_bounds_ = item_target_bounds;

  if (item_target_bounds.x == 0) {
    item_target_bounds.w += kItemSelectedHighlightedSize;
  } else if (item_target_bounds.x + item_target_bounds.w +
                 kItemSelectedHighlightedSize >
             bounds().w) {
    int diff = item_target_bounds.x + item_target_bounds.w +
               kItemSelectedHighlightedSize - bounds().w;
    item_target_bounds.x -= kItemSelectedHighlightedSize;
    item_target_bounds.w += kItemSelectedHighlightedSize * 2 - diff;
  } else {
    item_target_bounds.x -= kItemSelectedHighlightedSize;
    item_target_bounds.w += kItemSelectedHighlightedSize * 2;
  }

  if (item_target_bounds.y == 0) {
    item_target_bounds.h += kItemSelectedHighlightedSize;
  } else {
    item_target_bounds.y -= kItemSelectedHighlightedSize;
    item_target_bounds.h += kItemSelectedHighlightedSize * 2;
  }

  if (target_view_scrolling_ + item_target_bounds.y + item_target_bounds.h >
      bounds().h) {
    if (option == LayoutOption::kAdjustScrolling) {
      target_view_scrolling_ =
          bounds().h - (item_target_bounds.y + item_target_bounds.h);
    }
    current_index_exceeded_bottom = true;
  } else if (target_view_scrolling_ + item_target_bounds.y < 0 &&
             option == LayoutOption::kAdjustScrolling) {
    target_view_scrolling_ = -item_target_bounds.y;
  }

  current_item_original_bounds_.y += target_view_scrolling_;
  current_item_target_bounds_ = item_target_bounds;
  current_item_target_bounds_.y += target_view_scrolling_;

  return current_index_exceeded_bottom;
}

void FlexItemsWidget::ResetAnimationTimers() {
  selection_item_timer_.Reset();
  scrolling_timer_.Reset();
  updating_view_scrolling_ = true;
}

void FlexItemsWidget::AdjustBottomRowItemsIfNeeded(LayoutOption option) {
  if (current_item_widget_ && current_item_widget_->row_index() == rows_) {
    if (option == LayoutOption::kAdjustScrolling) {
      target_view_scrolling_ += kItemSelectedHighlightedSize;
      current_item_target_bounds_.h -= kItemSelectedHighlightedSize;
      current_item_target_bounds_.y += kItemSelectedHighlightedSize;
      current_item_original_bounds_.y += kItemSelectedHighlightedSize;
    } else {
      current_item_target_bounds_.h -= kItemSelectedHighlightedSize;
    }
  }
}

bool FlexItemsWidget::HandleInputEvent(SDL_KeyboardEvent* k,
                                       SDL_ControllerButtonEvent* c) {
  if (!activate_)
    return false;

  if (k || c) {
    StopInertialScrolling();
  }

  // Controller events are dispatched to the parent before its children.
  // While search is active, prevent the game list from moving; FilterWidget
  // will consume the same event when Widget dispatch continues to children.
  if (c && filter_widget_->has_begun())
    return true;

  if (k) {
    if (k->keysym.sym == SDLK_f) {
      ShowFilterWidget();
      return true;
    } else if (k->keysym.sym == SDLK_ESCAPE) {
      if (!filter_contents_.empty()) {
        OnFilter(std::string());
        return true;
      }
    }
  }

  constexpr Uint16 kCtrlAltShiftGuiMod =
      KMOD_CTRL | KMOD_ALT | KMOD_SHIFT | KMOD_GUI;
  if (k && (k->keysym.mod & kCtrlAltShiftGuiMod)) {
    // If any modifier (CTRL, ATL, SHIFT, GUI(Command, etc)) is pressed, we
    // won't treat this key event as handled, because many shortcut such as
    // 'Command+W' will close the application. See SDL_Keymod for more details.
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kLeft, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
    size_t next_index = FindNextIndex(kLeft);
    if (next_index != current_index_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      SetIndex(next_index);
    } else {
      back_callback_.Run();
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kB, k) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_X) ||
      (k && k->keysym.sym == SDLK_ESCAPE)) {
    back_callback_.Run();
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kRight, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
    size_t next_index = FindNextIndex(kRight);
    if (next_index != current_index_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      SetIndex(next_index);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kUp, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
    size_t next_index = FindNextIndex(kUp);
    if (next_index != current_index_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      SetIndex(next_index);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kDown, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
    size_t next_index = FindNextIndex(kDown);
    if (next_index != current_index_) {
      PlayEffect(audio_resources::AudioID::kSelect);
      SetIndex(next_index);
    }
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kStart, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_START ||
      IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kA, k) ||
      c && c->button == SDL_CONTROLLER_BUTTON_A) {
    if (TriggerCurrentItem(false))
      PlayEffect(audio_resources::AudioID::kStart);
    return true;
  }

  if (IsKeyboardOrControllerAxisMotionMatch(
          runtime_data_, kiwi::nes::ControllerButton::kSelect, k) ||
      (c && c->button == SDL_CONTROLLER_BUTTON_Y)) {
    if (!items_.empty() && items_[current_index_]->has_sub_items()) {
      PlayEffect(audio_resources::AudioID::kSelect);
      SwapCurrentItemToNextSubItem();
    }
    return true;
  }

  return false;
}

bool FlexItemsWidget::TriggerCurrentItem(bool triggered_by_finger) {
  if (!items_.empty()) {
    items_[current_index_]->Trigger(triggered_by_finger);
    RestoreCurrentItemToDefault();
    return true;
  }
  return false;
}

void FlexItemsWidget::ApplyScrolling(int scrolling) {
  const SDL_Rect kLocalBounds = GetLocalBounds();
  if (SDL_RectEmpty(&kLocalBounds))
    return;

  int first_visible_row = rows_ + 1;
  int last_visible_row = -1;
  for (auto* item : items_) {
    SDL_Rect bounds = bounds_map_without_scrolling_[item];
    bounds.y += scrolling;
    item->set_bounds(bounds);
    if (SDL_HasIntersection(&bounds, &kLocalBounds)) {
      first_visible_row = std::min(first_visible_row, item->row_index());
      last_visible_row = std::max(last_visible_row, item->row_index());
    }
  }

  if (last_visible_row < 0)
    return;

  const int first_cached_row =
      std::max(0, first_visible_row - kTextureCacheMarginRows);
  const int last_cached_row =
      std::min(rows_, last_visible_row + kTextureCacheMarginRows);
  for (auto* item : items_) {
    const bool cached = item->row_index() >= first_cached_row &&
                        item->row_index() <= last_cached_row;
    item->set_visible(cached);
    if (!cached)
      item->EvictImageTextures();
  }
}

size_t FlexItemsWidget::FindNextIndex(Direction direction) {
  switch (direction) {
    case kUp:
      return FindNextIndex(false);
    case kDown:
      return FindNextIndex(true);
    case kLeft: {
      if (current_index_ == 0)
        return 0;

      size_t next_index_candidate = current_index_ - 1;
      return (items_[next_index_candidate]->row_index() !=
              items_[current_index_]->row_index())
                 ? current_index_
                 : next_index_candidate;
    }
    case kRight: {
      if (current_index_ == items_.size() - 1)
        return items_.size() - 1;

      size_t next_index_candidate = current_index_ + 1;
      return (items_[next_index_candidate]->row_index() !=
              items_[current_index_]->row_index())
                 ? current_index_
                 : next_index_candidate;
    }
    default:
      SDL_assert(false);  // Shouldn't be here
      return 0;
  }
}

size_t FlexItemsWidget::FindNextIndex(bool down) {
  if (items_.empty())
    return 0;

  int area = 0, last_area = 0;
  int start_index, end_index;
  const int kCurrentRow = items_[current_index_]->row_index();
  if (down) {
    if (kCurrentRow == rows_)
      return current_index_;

    SDL_assert(kCurrentRow < rows_);
    start_index = rows_to_first_item_[kCurrentRow + 1];
    if (kCurrentRow < rows_ - 1)
      end_index = rows_to_first_item_[kCurrentRow + 2] - 1;
    else
      end_index = items_.size() - 1;
  } else {
    if (kCurrentRow == 0)
      return current_index_;

    SDL_assert(kCurrentRow > 0);
    start_index = rows_to_first_item_[kCurrentRow - 1];
    end_index = rows_to_first_item_[kCurrentRow] - 1;
  }

  int target_index = end_index;
  for (int i = start_index; i <= end_index; ++i) {
    SDL_assert(i >= 0 && i <= items_.size());
    int intersection_area =
        CalculateIntersectionArea(bounds_map_without_scrolling_[items_[i]],
                                  current_item_original_bounds_);

    if (intersection_area < 0)
      continue;

    area = intersection_area;
    if (area > last_area) {
      last_area = area;
      target_index = i;
    }
  }
  return target_index;
}

bool FlexItemsWidget::FindItemIndexByMousePosition(int x_in_window,
                                                   int y_in_window,
                                                   size_t& index_out) {
  if (items_.empty())
    return false;

  // A faster way to find the hovered item.
  int current_row_index = 0;
  if (current_item_widget_) {
    SDL_assert(!rows_to_first_item_.empty());
    current_row_index = current_item_widget_->row_index();
  }

  int r = current_row_index;
  int first_item_of_row = rows_to_first_item_[r];
  while (first_item_of_row > 0 &&
         (items_[first_item_of_row]->bounds().y +
          items_[first_item_of_row]->bounds().h) >= 0) {
    --r;
    first_item_of_row = rows_to_first_item_[r];
  }
  int index_lower = first_item_of_row;

  r = current_row_index;
  first_item_of_row = rows_to_first_item_[r];
  bool broke = false;
  while (items_[first_item_of_row]->bounds().y <= bounds().h) {
    ++r;
    if (rows_to_first_item_.find(r) == rows_to_first_item_.end()) {
      broke = true;
      break;
    }
    first_item_of_row = rows_to_first_item_[r];
  }
  int index_upper = broke ? items_.size() - 1 : first_item_of_row;

  for (int i = index_lower; i <= index_upper; ++i) {
    SDL_Rect item_bounds_to_window = MapToWindow(items_[i]->bounds());
    if (Contains(item_bounds_to_window, x_in_window, y_in_window)) {
      index_out = i;
      return true;
    }
  }

  return false;
}

void FlexItemsWidget::SwapCurrentItemToNextSubItem() {
  if (items_[current_index_]->SwapToNextSubItem()) {
    RefreshCurrentItemBounds();
  }
}

void FlexItemsWidget::RestoreCurrentItemToDefault() {
  if (!items_.empty() && items_[current_index_]->RestoreToDefaultItem()) {
    RefreshCurrentItemBounds();
  }
}

void FlexItemsWidget::RefreshCurrentItemBounds() {
  FlexItemWidget* item = items_[current_index_];
  SDL_Rect item_bounds =
      items_[current_index_]->GetSuggestedSize(kItemHeightHint);

  // The new bound is center aligned
  const SDL_Rect& kBounds = bounds_map_without_scrolling_[item];
  const int kMiddleX = kBounds.x + kBounds.w / 2;
  item_bounds.x = kMiddleX - item_bounds.w / 2;
  item_bounds.y = kBounds.y;
  HighlightItem(item, LayoutOption::kDoNotAdjustScrolling, item_bounds);
  ResetAnimationTimers();
}

void FlexItemsWidget::PaintDetails() {
  if (items_.empty())
    return;

  FlexItemWidget* item = items_[current_index_];
  const std::string title =
      item->current_data()->title_updater->GetLocalizedString();
  const std::string& selection_label =
      GetLocalizedString(string_resources::IDR_ITEMS_WIDGET_CURRENT_SELECTION);

  ScopedFont title_font(GetPreferredFont(
      styles::flex_items_widget::GetDetailFontSize(), title.c_str()));
  ScopedFont meta_font(
      GetPreferredFont(styles::flex_items_widget::GetDetailMetaFontSize(),
                       selection_label.c_str()));
  ImFont* title_im_font = title_font.GetFont();
  ImFont* meta_im_font = meta_font.GetFont();
  const float title_font_size = title_font.GetFontSize();
  const float meta_font_size = meta_font.GetFontSize();
  const ImVec2 natural_title_size = title_im_font->CalcTextSizeA(
      title_font_size, FLT_MAX, 0.f, title.c_str());
  const ImVec2 selection_label_size = meta_im_font->CalcTextSizeA(
      meta_font_size, FLT_MAX, 0.f, selection_label.c_str());

  const int desired_content_width = static_cast<int>(
      std::ceil(std::max(natural_title_size.x, selection_label_size.x)));
  const int available_panel_width =
      std::max(1, bounds().w - kDetailWidgetMargin * 2);
  const int max_panel_width = std::min(
      available_panel_width,
      std::max(1, static_cast<int>(bounds().w * kDetailWidgetMaxWidthRatio)));
  const int panel_width = std::min(
      max_panel_width, desired_content_width + kDetailWidgetPadding * 2 +
                           kDetailWidgetAccentWidth);
  const float title_wrap_width = std::max(
      1, panel_width - kDetailWidgetPadding * 2 - kDetailWidgetAccentWidth);
  const ImVec2 title_size = title_im_font->CalcTextSizeA(
      title_font_size, FLT_MAX, title_wrap_width, title.c_str());
  const int panel_height =
      static_cast<int>(std::ceil(selection_label_size.y + title_size.y)) +
      kDetailWidgetPadding * 2 + kDetailWidgetLineSpacing;
  const int panel_x = bounds().w - kDetailWidgetMargin - panel_width;
  const SDL_Rect panel_bounds_top =
      MapToWindow({panel_x, kDetailWidgetMargin, panel_width, panel_height});
  const SDL_Rect panel_bounds_bottom =
      MapToWindow({panel_x, bounds().h - kDetailWidgetMargin - panel_height,
                   panel_width, panel_height});

  const SDL_Rect* panel_bounds = nullptr;
  if (last_detail_widget_position_ == kTop) {
    if (Intersect(MapToWindow(current_item_target_bounds_), panel_bounds_top)) {
      panel_bounds = &panel_bounds_bottom;
      last_detail_widget_position_ = kBottom;
    } else {
      panel_bounds = &panel_bounds_top;
    }
  } else {
    if (Intersect(MapToWindow(current_item_target_bounds_),
                  panel_bounds_bottom)) {
      panel_bounds = &panel_bounds_top;
      last_detail_widget_position_ = kTop;
    } else {
      panel_bounds = &panel_bounds_bottom;
    }
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  const ImVec2 panel_min(panel_bounds->x, panel_bounds->y);
  const ImVec2 panel_max(panel_bounds->x + panel_bounds->w,
                         panel_bounds->y + panel_bounds->h);
  draw_list->AddRectFilled(panel_min, panel_max, kDetailBackgroundColor,
                           kDetailWidgetCornerRadius);
  draw_list->AddRect(panel_min, panel_max, kDetailBorderColor,
                     kDetailWidgetCornerRadius);
  draw_list->AddRectFilled(
      panel_min, ImVec2(panel_min.x + kDetailWidgetAccentWidth, panel_max.y),
      kDetailAccentColor, kDetailWidgetCornerRadius,
      ImDrawFlags_RoundCornersLeft);

  const float text_left =
      panel_min.x + kDetailWidgetAccentWidth + kDetailWidgetPadding;
  float text_top = panel_min.y + kDetailWidgetPadding;
  draw_list->AddText(meta_im_font, meta_font_size, ImVec2(text_left, text_top),
                     kDetailMetaColor, selection_label.c_str());
  text_top += selection_label_size.y + kDetailWidgetLineSpacing;

  const ImVec4 title_clip(text_left, text_top,
                          panel_max.x - kDetailWidgetPadding,
                          text_top + title_size.y);
  draw_list->AddText(title_im_font, title_font_size,
                     ImVec2(text_left, text_top), kDetailTextColor,
                     title.c_str(), nullptr, title_wrap_width, &title_clip);
}

void FlexItemsWidget::PaintFilter() {
  if (!filter_contents_.empty()) {
#if KIWI_MOBILE
    std::string template_string =
        GetLocalizedString(string_resources::IDR_ITEMS_WIDGET_FILTERING_MOBILE);
#else
    std::string template_string =
        GetLocalizedString(string_resources::IDR_ITEMS_WIDGET_FILTERING);
#endif
    std::string filter_contents = kiwi::base::StringPrintf(
        template_string.c_str(), filter_contents_.c_str());

    PreferredFontSize preferred_font_size =
        styles::flex_items_widget::GetFilterFontSize();
    ScopedFont font =
        GetPreferredFont(preferred_font_size, filter_contents.c_str());
    ImVec2 text_size = ImGui::CalcTextSize(filter_contents.c_str());
    SDL_Rect text_bounds_top =
        MapToWindow({kFilterWidgetMargin, kFilterWidgetMargin,
                     static_cast<int>(text_size.x + kFilterWidgetPadding * 2),
                     static_cast<int>(text_size.y + kFilterWidgetPadding * 2)});
    SDL_Rect text_bounds_bottom =
        MapToWindow({kFilterWidgetMargin,
                     static_cast<int>(bounds().h - kFilterWidgetMargin -
                                      kFilterWidgetPadding * 2 - text_size.y),
                     static_cast<int>(text_size.x + kFilterWidgetPadding * 2),
                     static_cast<int>(text_size.y + kFilterWidgetPadding * 2)});

    const SDL_Rect* kTextBounds = nullptr;
    if (last_detail_widget_position_ == kTop) {
      if (Intersect(MapToWindow(current_item_target_bounds_),
                    text_bounds_top)) {
        kTextBounds = &text_bounds_bottom;
        last_detail_widget_position_ = kBottom;
      } else {
        kTextBounds = &text_bounds_top;
      }
    } else {
      if (Intersect(MapToWindow(current_item_target_bounds_),
                    text_bounds_bottom)) {
        kTextBounds = &text_bounds_top;
        last_detail_widget_position_ = kTop;
      } else {
        kTextBounds = &text_bounds_bottom;
      }
    }

    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(kTextBounds->x, kTextBounds->y),
        ImVec2(kTextBounds->x + kTextBounds->w,
               kTextBounds->y + kTextBounds->h),
        ImColor(0.f, 0.f, 0.f, .7f));
    ImGui::GetWindowDrawList()->AddText(
        font.GetFont(), font.GetFontSize(),
        ImVec2(kTextBounds->x + kDetailWidgetPadding,
               kTextBounds->y + kDetailWidgetPadding),
        ImColor(1.f, 1.f, 1.f), filter_contents.c_str());
  }
}

bool FlexItemsWidget::HandleMouseOrFingerEvents(MouseOrFingerEventType type,
                                                MouseButton button,
                                                int x_in_window,
                                                int y_in_window) {
  if (filter_widget_->has_begun())
    return true;

  switch (type) {
    case MouseOrFingerEventType::kHover: {
      if (!mouse_moved_ && !scrolling_by_finger_) {
        gesture_locked_ = false;
        if (items_[current_index_]->has_sub_items()) {
          PlayEffect(audio_resources::AudioID::kSelect);
          SwapCurrentItemToNextSubItem();
        }
      }
      return true;
    }
    case MouseOrFingerEventType::kMousePressed:
    case MouseOrFingerEventType::kFingerDown: {
      mouse_moved_ = false;
      gesture_locked_ = true;
      gesture_locked_timer_.Reset();
      gesture_locked_button_ = button;
      return true;
    }
    case MouseOrFingerEventType::kFingerUp: {
      AutoReset auto_reset(gesture_locked_);
      if (activate_) {
        if (gesture_locked_) {
          // If the widget is scrolling by finger gesture, do not trigger the
          // item until finger up.
          if (!scrolling_by_finger_) {
            size_t index = 0;
            if (FindItemIndexByMousePosition(x_in_window, y_in_window, index)) {
              if (current_index_ == index) {
                HandleTriggerItemByLeftMouseButtonDownOrFingerUp(
                    x_in_window, y_in_window, true);
                return true;
              }

              size_t select_index = 0;
              if (FindItemIndexByMousePosition(x_in_window, y_in_window,
                                               select_index))
                SetIndex(index, LayoutOption::kAdjustScrolling, false);
            }
          }
        }
      } else {
        // Prevents finger up events triggered by other widget.
        if (gesture_locked_) {
          PlayEffect(audio_resources::AudioID::kSelect);
          main_window_->ChangeFocus(MainWindow::MainFocus::kContents);
        }
      }
      return true;
    }
    case MouseOrFingerEventType::kMouseMove: {
      mouse_moved_ = true;
      if (!activate_ || gesture_locked_)
        return true;

      size_t index = 0;
      if (FindItemIndexByMousePosition(x_in_window, y_in_window, index))
        SetIndex(index, LayoutOption::kDoNotAdjustScrolling, false);

      return true;
    }
    case MouseOrFingerEventType::kMouseReleased: {
      AutoReset auto_reset(gesture_locked_);
      if (activate_) {
        if (gesture_locked_) {
          if (button == MouseButton::kLeftButton) {
            HandleTriggerItemByLeftMouseButtonDownOrFingerUp(
                x_in_window, y_in_window, false);
          } else if (button == MouseButton::kRightButton) {
            back_callback_.Run();
          }
        }
      } else {
        // Prevents mouse released events triggered by other widget.
        if (gesture_locked_) {
          PlayEffect(audio_resources::AudioID::kSelect);
          main_window_->ChangeFocus(MainWindow::MainFocus::kContents);
        }
      }
      return true;
    }
    default:
      return false;
  }
}

void FlexItemsWidget::HandleTriggerItemByLeftMouseButtonDownOrFingerUp(
    int x_in_window,
    int y_in_window,
    bool is_finger_gesture) {
  size_t index_before_released = current_index_;
  size_t index = 0;
  if (FindItemIndexByMousePosition(x_in_window, y_in_window, index)) {
    // If the highlight item doesn't change, trigger it.
    if (index_before_released == index) {
      if (TriggerCurrentItem(is_finger_gesture))
        PlayEffect(audio_resources::AudioID::kStart);
    } else {
      SetIndex(index, LayoutOption::kDoNotAdjustScrolling, false);
    }
  }
}

void FlexItemsWidget::Paint() {
  if (first_paint_) {
    Layout(LayoutOption::kAdjustScrolling);
    first_paint_ = false;
  }

  UpdateInertialScrolling();

  // Scrolling animation
  if (updating_view_scrolling_) {
    float percentage = scrolling_timer_.ElapsedInMilliseconds() /
                       static_cast<float>(kScrollingAnimationMs);
    if (percentage > 1.f) {
      percentage = 1.f;
      updating_view_scrolling_ = false;
    }
    int scrolling =
        Lerp(original_view_scrolling_, target_view_scrolling_, percentage);
    ApplyScrolling(scrolling);
  }

  // Selected item animation
  if (current_item_widget_) {
    float percentage = selection_item_timer_.ElapsedInMilliseconds() /
                       static_cast<float>(kItemAnimationMs);
    if (percentage > 1.f)
      percentage = 1.f;
    current_item_widget_->set_bounds(Lerp(current_item_original_bounds_,
                                          current_item_target_bounds_,
                                          percentage));
  }

  if (gesture_locked_ &&
      gesture_locked_timer_.ElapsedInMilliseconds() > kItemHoverDurationMs) {
    HandleMouseOrFingerEvents(MouseOrFingerEventType::kHover,
                              gesture_locked_button_, 0, 0);
  }

  SDL_Rect rect_in_window = MapToWindow(GetLocalBounds());
  DrawLibraryBackground(ImGui::GetWindowDrawList(), rect_in_window,
                        static_cast<float>(ImGui::GetTime()));
}

void FlexItemsWidget::EnsureUniqueFilterSearchIndex() {
  if (filter_search_index_.use_count() != 1) {
    filter_search_index_ =
        std::make_shared<FilterSearchIndex>(*filter_search_index_);
  }
}

void FlexItemsWidget::RebuildFilterSearchIndex() {
  auto search_index = std::make_shared<FilterSearchIndex>();
  search_index->reserve(all_items_.size());
  for (FlexItemWidget* item : all_items_)
    search_index->push_back(item->GetFilterStrings());
  filter_search_index_ = std::move(search_index);
}

void FlexItemsWidget::OnFilter(const std::string& filter) {
  if (filter_contents_ == filter)
    return;

  filter_contents_ = filter;
  const uint64_t request_id =
      filter_request_id_->fetch_add(1, std::memory_order_relaxed) + 1;

  if (filter.empty()) {
    std::vector<size_t> all_item_indices;
    all_item_indices.reserve(all_items_.size());
    for (size_t i = 0; i < all_items_.size(); ++i)
      all_item_indices.push_back(i);
    ApplyFilteredResult(request_id, all_item_indices);
    return;
  }

  std::shared_ptr<const FilterSearchIndex> search_index = filter_search_index_;
  std::weak_ptr<int> weak_lifetime = filter_lifetime_token_;
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&CalculateFilteredResultOnIOThread,
                           std::move(search_index), filter_request_id_,
                           request_id, filter),
      kiwi::base::BindOnce(&FlexItemsWidget::DispatchFilteredResult,
                           std::move(weak_lifetime), this, request_id));
}

void FlexItemsWidget::DispatchFilteredResult(
    std::weak_ptr<int> weak_lifetime,
    FlexItemsWidget* widget,
    uint64_t request_id,
    const std::vector<size_t>& item_indices) {
  if (!weak_lifetime.expired())
    widget->ApplyFilteredResult(request_id, item_indices);
}

void FlexItemsWidget::ApplyFilteredResult(
    uint64_t request_id,
    const std::vector<size_t>& item_indices) {
  if (filter_request_id_->load(std::memory_order_relaxed) != request_id)
    return;

  RestoreCurrentItemToDefault();
  items_.clear();
  items_.reserve(item_indices.size());

  std::vector<bool> item_is_filtered(all_items_.size(), true);
  for (size_t item_index : item_indices) {
    if (item_index >= all_items_.size())
      continue;
    item_is_filtered[item_index] = false;
    items_.push_back(all_items_[item_index]);
  }

  for (size_t i = 0; i < all_items_.size(); ++i) {
    all_items_[i]->set_filtered(item_is_filtered[i]);
    if (item_is_filtered[i])
      all_items_[i]->EvictImageTextures();
  }

  original_view_scrolling_ = target_view_scrolling_ = 0;
  need_layout_all_ = true;
  current_index_ = 0;
  SetIndex(0, LayoutOption::kAdjustScrolling, true);
}

void FlexItemsWidget::PostPaint() {
  // Renders current item again, to make it on the top of all the items.
  if (current_item_widget_) {
    current_item_widget_->Render();
  }

  PaintDetails();
  PaintFilter();

  if (filter_widget_->visible())
    filter_widget_->Render();

  if (!activate_) {
    SDL_Rect bounds_to_window = MapToWindow(bounds());
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(bounds_to_window.x, bounds_to_window.y),
        ImVec2(bounds_to_window.x + bounds_to_window.w,
               bounds_to_window.y + bounds_to_window.h),
        ImColor(0, 0, 0, 196));
  }
}

void FlexItemsWidget::OnWindowResized() {
  need_layout_all_ = true;
  Layout(LayoutOption::kAdjustScrolling);
}

void FlexItemsWidget::OnLocaleChanged() {
  RebuildFilterSearchIndex();
  filter_request_id_->fetch_add(1, std::memory_order_relaxed);
  if (!filter_contents_.empty()) {
    std::string filter = std::move(filter_contents_);
    filter_contents_.clear();
    OnFilter(filter);
  }
}

bool FlexItemsWidget::OnKeyPressed(SDL_KeyboardEvent* event) {
  return HandleInputEvent(event, nullptr);
}

bool FlexItemsWidget::OnMouseMove(SDL_MouseMotionEvent* event) {
  if (filter_widget_->OnMouseMove(event))
    return true;

  return HandleMouseOrFingerEvents(MouseOrFingerEventType::kMouseMove,
                                   MouseButton::kUnknownButton, event->x,
                                   event->y);
}

bool FlexItemsWidget::OnMouseWheel(SDL_MouseWheelEvent* event) {
  if (filter_widget_->OnMouseWheel(event))
    return true;

  if (!activate_ || gesture_locked_)
    return true;

#if BUILDFLAG(IS_MAC)
  constexpr float kScrollingTurbo = 5.f;

  const float precise_delta = event->preciseY * kScrollingTurbo;
  if (precise_delta * wheel_scroll_remainder_ < 0.f)
    wheel_scroll_remainder_ = 0.f;
  const float scroll_delta = precise_delta + wheel_scroll_remainder_;
  const int scrolling_changed_value = static_cast<int>(scroll_delta);
  wheel_scroll_remainder_ = scroll_delta - scrolling_changed_value;

  if (wheel_gesture_active_ && std::abs(precise_delta) > 0.f) {
    const Uint32 elapsed_ms = event->timestamp - last_wheel_motion_timestamp_;
    const bool direction_changed =
        precise_delta * last_wheel_scroll_delta_ < 0.f;
    if (last_wheel_motion_timestamp_ == 0 ||
        elapsed_ms > kWheelVelocitySampleTimeoutMs || direction_changed) {
      wheel_scroll_velocity_ = 0.f;
      has_wheel_velocity_sample_ = false;
    } else if (elapsed_ms > 0) {
      const float velocity =
          std::clamp(precise_delta * 1000.f / elapsed_ms,
                     -kMaximumWheelFlingVelocity, kMaximumWheelFlingVelocity);
      if (has_wheel_velocity_sample_) {
        wheel_scroll_velocity_ +=
            (velocity - wheel_scroll_velocity_) * kWheelVelocitySmoothing;
      } else {
        wheel_scroll_velocity_ = velocity;
        has_wheel_velocity_sample_ = true;
      }
    }
    last_wheel_scroll_delta_ = precise_delta;
    last_wheel_motion_timestamp_ = event->timestamp;
  }
#else
  constexpr int kScrollingTurbo = 25;
  const int scrolling_changed_value = event->preciseY * kScrollingTurbo;
#endif
  if (!items_.empty() && scrolling_changed_value != 0)
    ScrollWith(scrolling_changed_value, &event->mouseX, &event->mouseY);

  return true;
}

bool FlexItemsWidget::OnMouseWheelPhase(MouseWheelPhaseEvent* event) {
#if BUILDFLAG(IS_MAC)
  if (!activate_)
    return true;

  switch (event->phase) {
    case MouseWheelPhase::kBegin:
      StopInertialScrolling();
      wheel_scroll_remainder_ = 0.f;
      wheel_scroll_velocity_ = 0.f;
      last_wheel_scroll_delta_ = 0.f;
      last_wheel_motion_timestamp_ = event->timestamp;
      has_wheel_velocity_sample_ = false;
      wheel_gesture_active_ = true;
      break;
    case MouseWheelPhase::kEnd: {
      const bool has_recent_velocity =
          has_wheel_velocity_sample_ &&
          event->timestamp - last_wheel_motion_timestamp_ <=
              kWheelVelocitySampleTimeoutMs;
      const float release_velocity = wheel_scroll_velocity_;
      wheel_scroll_remainder_ = 0.f;
      wheel_scroll_velocity_ = 0.f;
      last_wheel_scroll_delta_ = 0.f;
      last_wheel_motion_timestamp_ = 0;
      has_wheel_velocity_sample_ = false;
      wheel_gesture_active_ = false;

      if (has_recent_velocity)
        StartInertialScrolling(release_velocity);
      else
        StopInertialScrolling();
      break;
    }
    case MouseWheelPhase::kCancel:
      StopInertialScrolling();
      wheel_scroll_remainder_ = 0.f;
      wheel_scroll_velocity_ = 0.f;
      last_wheel_scroll_delta_ = 0.f;
      last_wheel_motion_timestamp_ = 0;
      has_wheel_velocity_sample_ = false;
      wheel_gesture_active_ = false;
      break;
    case MouseWheelPhase::kNativeMomentum:
      StopInertialScrolling();
      wheel_scroll_velocity_ = 0.f;
      last_wheel_scroll_delta_ = 0.f;
      last_wheel_motion_timestamp_ = 0;
      has_wheel_velocity_sample_ = false;
      wheel_gesture_active_ = false;
      break;
  }
#endif
  return true;
}

bool FlexItemsWidget::OnMousePressed(SDL_MouseButtonEvent* event) {
  if (filter_widget_->OnMousePressed(event))
    return true;

  MouseButton button = (event->button == SDL_BUTTON_LEFT
                            ? MouseButton::kLeftButton
                            : ((event->button == SDL_BUTTON_RIGHT
                                    ? MouseButton::kRightButton
                                    : MouseButton::kUnknownButton)));
  return HandleMouseOrFingerEvents(MouseOrFingerEventType::kMousePressed,
                                   button, event->x, event->y);
}

bool FlexItemsWidget::OnMouseReleased(SDL_MouseButtonEvent* event) {
  if (filter_widget_->OnMouseReleased(event))
    return true;

  MouseButton button = (event->button == SDL_BUTTON_LEFT
                            ? MouseButton::kLeftButton
                            : ((event->button == SDL_BUTTON_RIGHT
                                    ? MouseButton::kRightButton
                                    : MouseButton::kUnknownButton)));
  return HandleMouseOrFingerEvents(MouseOrFingerEventType::kMouseReleased,
                                   button, event->x, event->y);
}

bool FlexItemsWidget::OnControllerButtonPressed(
    SDL_ControllerButtonEvent* event) {
  return HandleInputEvent(nullptr, event);
}

bool FlexItemsWidget::OnControllerAxisMotionEvent(
    SDL_ControllerAxisEvent* event) {
  return HandleInputEvent(nullptr, nullptr);
}

void FlexItemsWidget::OnWindowPreRender() {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
}

void FlexItemsWidget::OnWindowPostRender() {
  ImGui::PopStyleVar(2);
}

int FlexItemsWidget::GetHitTestPolicy() {
  // Children don't accept any hit test, and mouse events. All mouse events are
  // handled by this FlexItemsWidget.
  return Widget::GetHitTestPolicy() & (~kChildrenAcceptHitTest);
}
