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

#include "ui/widgets/flex_item_widget.h"

#include <SDL_image.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <iterator>

#include "ui/application.h"
#include "ui/main_window.h"
#include "ui/styles.h"
#include "ui/widgets/flex_items_widget.h"
#include "utility/algorithm.h"
#include "utility/math.h"

namespace {
constexpr float kCoverHWRatio = 250.f / 200;
constexpr float kFadeDurationInMs = 1000;
constexpr float kVersionSwitchDesignSize = 48.f;
constexpr float kVersionSwitchAnimationSplit = .34f;
constexpr int kVersionSwitchAnimationMs = 520;
constexpr int kVersionSwitchIdleMs = 3200;
const int kVersionSwitchIconSize =
    styles::flex_item_widget::GetBadgeSize() * 5 / 4;
constexpr int kVersionSwitchIconMargin = 5;

constexpr ImU32 kVersionCardShadowColor = IM_COL32(0, 0, 0, 180);
constexpr ImU32 kVersionCardFaceColor = IM_COL32(5, 19, 29, 248);
constexpr ImU32 kVersionCardPrimaryColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kVersionCardAccentColor = IM_COL32(19, 218, 255, 255);

struct VersionCardPose {
  float x;
  float y;
  float opacity;
};

float SmoothStep(float progress) {
  progress = std::clamp(progress, 0.f, 1.f);
  return progress * progress * (3.f - 2.f * progress);
}

VersionCardPose LerpPose(const VersionCardPose& from,
                         const VersionCardPose& to,
                         float progress) {
  return {
      Lerp(from.x, to.x, progress),
      Lerp(from.y, to.y, progress),
      Lerp(from.opacity, to.opacity, progress),
  };
}

ImU32 ApplyOpacity(ImU32 color, float opacity) {
  const ImU32 original_alpha = (color & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
  const ImU32 alpha =
      static_cast<ImU32>(original_alpha * std::clamp(opacity, 0.f, 1.f));
  return (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
}

ImVec2 ScalePoint(const ImVec2& origin, float scale, float x, float y) {
  return ImVec2(origin.x + x * scale, origin.y + y * scale);
}

template <std::size_t N>
std::array<ImVec2, N> ScalePoints(const ImVec2& origin,
                                  float scale,
                                  const std::array<ImVec2, N>& points) {
  std::array<ImVec2, N> scaled_points;
  for (std::size_t i = 0; i < N; ++i) {
    scaled_points[i] = ScalePoint(origin, scale, points[i].x, points[i].y);
  }
  return scaled_points;
}

void DrawVersionCard(ImDrawList* draw_list,
                     const ImVec2& icon_origin,
                     float scale,
                     const VersionCardPose& pose,
                     ImU32 accent_color) {
  constexpr std::array<ImVec2, 5> kCardShape = {
      ImVec2(0.f, 0.f), ImVec2(19.f, 0.f), ImVec2(25.f, 6.f),
      ImVec2(25.f, 33.f), ImVec2(0.f, 33.f)};

  const ImVec2 card_origin = ScalePoint(icon_origin, scale, pose.x, pose.y);
  const ImVec2 shadow_origin = ScalePoint(card_origin, scale, 1.25f, 1.5f);
  const std::array<ImVec2, 5> shadow_points =
      ScalePoints(shadow_origin, scale, kCardShape);
  const std::array<ImVec2, 5> card_points =
      ScalePoints(card_origin, scale, kCardShape);
  const ImU32 shadow_color =
      ApplyOpacity(kVersionCardShadowColor, pose.opacity);
  const ImU32 face_color = ApplyOpacity(kVersionCardFaceColor, pose.opacity);
  const ImU32 line_color = ApplyOpacity(accent_color, pose.opacity);
  const float outline_width = std::max(1.f, 2.f * scale);
  const float detail_width = std::max(1.f, 1.7f * scale);

  draw_list->AddConvexPolyFilled(shadow_points.data(),
                                 static_cast<int>(shadow_points.size()),
                                 shadow_color);
  draw_list->AddConvexPolyFilled(
      card_points.data(), static_cast<int>(card_points.size()), face_color);
  draw_list->AddPolyline(card_points.data(),
                         static_cast<int>(card_points.size()), line_color,
                         ImDrawFlags_Closed, outline_width);
  draw_list->AddLine(ScalePoint(card_origin, scale, 5.f, 8.f),
                     ScalePoint(card_origin, scale, 18.f, 8.f), line_color,
                     detail_width);
  draw_list->AddLine(ScalePoint(card_origin, scale, 5.f, 14.f),
                     ScalePoint(card_origin, scale, 15.f, 14.f), line_color,
                     detail_width);
  draw_list->AddRectFilled(ScalePoint(card_origin, scale, 5.f, 24.f),
                           ScalePoint(card_origin, scale, 10.f, 29.f),
                           line_color);
}

SDL_Rect GetVersionSwitchIconBounds(const SDL_Rect& cover_bounds) {
  return {
      cover_bounds.x + cover_bounds.w - kVersionSwitchIconMargin -
          kVersionSwitchIconSize,
      cover_bounds.y + kVersionSwitchIconMargin,
      kVersionSwitchIconSize,
      kVersionSwitchIconSize,
  };
}
}  // namespace

FlexItemWidget::FlexItemWidget(
    MainWindow* main_window,
    FlexItemsWidget* parent,
    std::unique_ptr<LocalizedStringUpdater> title_updater,
    int image_width,
    int image_height,
    bool is_hd_edition,
    LoadImageCallback image_loader,
    TriggerCallback on_trigger)
    : Widget(main_window),
      main_window_(main_window),
      parent_(parent),
      loading_widget_(main_window_) {
  SDL_assert(parent_);

  // Initialize the default data
  std::unique_ptr<Data> default_data = std::make_unique<Data>();
  default_data->title_updater = std::move(title_updater);
  default_data->on_trigger_callback = std::move(on_trigger);
  default_data->image_loader = image_loader;
  default_data->image_width = image_width;
  default_data->image_height = image_height;
  default_data->is_hd_edition = is_hd_edition;
  current_data_ = default_data.get();
  sub_data_.push_back(std::move(default_data));

  // Since title won't change during the instance created, calculates the font
  // once to improve performance.
  SDL_assert(current_data()->title_updater);
}

FlexItemWidget::~FlexItemWidget() {
  EvictImageTextures();
}

void FlexItemWidget::EvictImageTextures() {
  for (std::unique_ptr<Data>& data : sub_data_) {
    if (!data->requesting_or_requested_texture_ &&
        !data->image_texture.load()) {
      continue;
    }

    ++data->texture_request_generation;
    data->requesting_or_requested_texture_ = false;
    if (SDL_Texture* texture = data->image_texture.exchange(nullptr))
      SDL_DestroyTexture(texture);
  }
}

void FlexItemWidget::CreateTextureIfNotExists() {
  Data* image_data = current_data();
  if (!image_data->image_texture &&
      !image_data->requesting_or_requested_texture_) {
    // Make sure we only request once
    image_data->requesting_or_requested_texture_ = true;
    const uint64_t request_generation =
        ++image_data->texture_request_generation;

    // If there's no texture yet, post a task to request a new one.
    Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE, image_data->image_loader,
        kiwi::base::BindOnce(
            [](FlexItemWidget* this_widget, Data* target_data,
               uint64_t request_generation, const kiwi::nes::Bytes& data) {
              if (target_data->texture_request_generation !=
                  request_generation) {
                return;
              }
              SDL_Texture* texture =
                  this_widget->LoadImageAndCreateTexture(data);
              target_data->image_texture.exchange(texture);
            },
            this, image_data, request_generation));
  }
}

SDL_Texture* FlexItemWidget::LoadImageAndCreateTexture(
    const kiwi::nes::Bytes& data) {
  SDL_RWops* bg_res =
      SDL_RWFromConstMem(const_cast<unsigned char*>(data.data()), data.size());
  SDL_Texture* image_texture =
      IMG_LoadTextureTyped_RW(window()->renderer(), bg_res, 1, nullptr);
  SDL_SetTextureScaleMode(image_texture, SDL_ScaleModeBest);
  return image_texture;
}

bool FlexItemWidget::MatchFilter(const std::string& filter,
                                 int& similarity) const {
  for (const auto& sub_data : sub_data_) {
    if (sub_data->title_updater->IsTitleMatchedFilter(filter, similarity))
      return true;
  }
  return false;
}

std::vector<std::string> FlexItemWidget::GetFilterStrings() const {
  std::vector<std::string> filter_strings;
  for (const auto& sub_data : sub_data_) {
    std::vector<std::string> data_filter_strings =
        sub_data->title_updater->GetFilterStrings();
    filter_strings.insert(filter_strings.end(),
                          std::make_move_iterator(data_filter_strings.begin()),
                          std::make_move_iterator(data_filter_strings.end()));
  }
  return filter_strings;
}

SDL_Rect FlexItemWidget::GetSuggestedSize(int item_height) {
  SDL_Rect bs = bounds();
  bs.h = item_height;
  bs.w = bs.h * current_data()->image_width / current_data()->image_height;
  return bs;
}

void FlexItemWidget::Trigger(bool triggered_by_finger) {
  if (current_data()->on_trigger_callback)
    current_data()->on_trigger_callback.Run(triggered_by_finger);
}

void FlexItemWidget::AddSubItem(
    std::unique_ptr<LocalizedStringUpdater> title_updater,
    int image_width,
    int image_height,
    bool is_hd_edition,
    LoadImageCallback image_loader,
    TriggerCallback on_trigger) {
  std::unique_ptr<Data> data = std::make_unique<Data>();
  data->image_width = image_width;
  data->image_height = image_height;
  data->is_hd_edition = is_hd_edition;
  data->image_loader = image_loader;
  data->title_updater = std::move(title_updater);
  data->on_trigger_callback = on_trigger;
  sub_data_.push_back(std::move(data));
}

bool FlexItemWidget::IsPointInVersionSwitchIcon(int x_in_window,
                                                int y_in_window) {
  if (!has_sub_items())
    return false;

  const SDL_Rect cover_bounds = MapToWindow(bounds());
  SDL_Rect hit_bounds = GetVersionSwitchIconBounds(cover_bounds);
  const int hit_padding = std::max(4, kVersionSwitchIconSize / 8);
  hit_bounds.x -= hit_padding;
  hit_bounds.y -= hit_padding;
  hit_bounds.w += hit_padding * 2;
  hit_bounds.h += hit_padding * 2;
  SDL_Rect clipped_bounds;
  return SDL_IntersectRect(&hit_bounds, &cover_bounds, &clipped_bounds) &&
         Contains(clipped_bounds, x_in_window, y_in_window);
}

void FlexItemWidget::SetVersionSwitchIconPressed(bool pressed) {
  version_switch_icon_pressed_ = pressed;
}

bool FlexItemWidget::RestoreToDefaultItem() {
  bool changed = current_sub_item_index_ != 0;
  if (changed) {
    current_sub_item_index_ = 0;
    current_data_ = sub_data_[current_sub_item_index_].get();
    version_switch_animating_ = false;
    version_switch_cards_swapped_ = false;
    version_switch_idle_timer_.Reset();
  }
  return changed;
}

bool FlexItemWidget::SwapToNextSubItem() {
  int sub_item_index_before = current_sub_item_index_;
  ++current_sub_item_index_;
  if (current_sub_item_index_ >= sub_data_.size())
    current_sub_item_index_ = 0;

  current_data_ = sub_data_[current_sub_item_index_].get();
  const bool changed = current_sub_item_index_ != sub_item_index_before;
  if (changed)
    StartVersionSwitchAnimation();
  return changed;
}

void FlexItemWidget::StartVersionSwitchAnimation() {
  if (!has_sub_items())
    return;

  if (version_switch_animating_ &&
      version_switch_animation_timer_.ElapsedInMilliseconds() >=
          kVersionSwitchAnimationMs * kVersionSwitchAnimationSplit) {
    version_switch_cards_swapped_ = !version_switch_cards_swapped_;
  }
  version_switch_animation_timer_.Reset();
  version_switch_idle_timer_.Reset();
  version_switch_animating_ = true;
}

void FlexItemWidget::PaintVersionSwitchIcon(ImDrawList* draw_list,
                                            const SDL_Rect& cover_bounds,
                                            bool is_selected) {
  SDL_Rect icon_bounds = GetVersionSwitchIconBounds(cover_bounds);
  const ImVec2 mouse_position = ImGui::GetIO().MousePos;
  const bool is_hovered =
      is_selected &&
      IsPointInVersionSwitchIcon(static_cast<int>(mouse_position.x),
                                 static_cast<int>(mouse_position.y));

  if (is_hovered != version_switch_icon_was_hovered_) {
    version_switch_idle_timer_.Reset();
  }
  version_switch_icon_was_hovered_ = is_hovered;

  if (!is_hovered && !version_switch_icon_pressed_ &&
      !version_switch_animating_ &&
      version_switch_idle_timer_.ElapsedInMilliseconds() >=
          kVersionSwitchIdleMs) {
    StartVersionSwitchAnimation();
  }

  float animation_progress = 0.f;
  if (version_switch_animating_) {
    const int elapsed_ms =
        version_switch_animation_timer_.ElapsedInMilliseconds();
    if (elapsed_ms >= kVersionSwitchAnimationMs) {
      version_switch_animating_ = false;
      version_switch_cards_swapped_ = !version_switch_cards_swapped_;
    } else {
      animation_progress =
          elapsed_ms / static_cast<float>(kVersionSwitchAnimationMs);
    }
  }

  const float input_scale = version_switch_icon_pressed_ ? .96f : 1.f;
  const float scale = static_cast<float>(icon_bounds.w) /
                      kVersionSwitchDesignSize * input_scale;
  const float inset =
      (static_cast<float>(icon_bounds.w) - kVersionSwitchDesignSize * scale) *
      .5f;
  const float hover_lift = is_hovered && !version_switch_icon_pressed_
                               ? std::max(1.f, icon_bounds.w / 32.f)
                               : 0.f;
  const ImVec2 icon_origin(icon_bounds.x + inset,
                           icon_bounds.y + inset - hover_lift);

  constexpr VersionCardPose kBackPose = {8.f, 6.f, .74f};
  constexpr VersionCardPose kFrontPose = {15.f, 10.f, 1.f};
  constexpr VersionCardPose kExitPose = {25.f, 11.f, .34f};
  VersionCardPose card_a =
      version_switch_cards_swapped_ ? kFrontPose : kBackPose;
  VersionCardPose card_b =
      version_switch_cards_swapped_ ? kBackPose : kFrontPose;
  const bool outgoing_is_card_a = version_switch_cards_swapped_;
  bool incoming_drawn_last = false;

  if (version_switch_animating_) {
    VersionCardPose* outgoing = outgoing_is_card_a ? &card_a : &card_b;
    VersionCardPose* incoming = outgoing_is_card_a ? &card_b : &card_a;
    if (animation_progress < kVersionSwitchAnimationSplit) {
      const float progress =
          SmoothStep(animation_progress / kVersionSwitchAnimationSplit);
      *outgoing = LerpPose(kFrontPose, kExitPose, progress);
      *incoming = kBackPose;
    } else {
      const float progress =
          SmoothStep((animation_progress - kVersionSwitchAnimationSplit) /
                     (1.f - kVersionSwitchAnimationSplit));
      *outgoing = kBackPose;
      outgoing->opacity = Lerp(kExitPose.opacity, kBackPose.opacity, progress);
      *incoming = LerpPose(kBackPose, kFrontPose, progress);
      incoming_drawn_last = true;
    }
  } else if (is_hovered) {
    VersionCardPose* front = version_switch_cards_swapped_ ? &card_a : &card_b;
    VersionCardPose* back = version_switch_cards_swapped_ ? &card_b : &card_a;
    const float hover_offset = 1.25f;
    front->x += hover_offset;
    front->y += hover_offset;
    back->x -= hover_offset;
    back->y -= hover_offset;
  }

  const ImVec2 clip_min(static_cast<float>(cover_bounds.x),
                        static_cast<float>(cover_bounds.y));
  const ImVec2 clip_max(static_cast<float>(cover_bounds.x + cover_bounds.w),
                        static_cast<float>(cover_bounds.y + cover_bounds.h));
  draw_list->PushClipRect(clip_min, clip_max, true);
  if (version_switch_animating_) {
    if (incoming_drawn_last) {
      if (outgoing_is_card_a) {
        DrawVersionCard(draw_list, icon_origin, scale, card_a,
                        kVersionCardPrimaryColor);
        DrawVersionCard(draw_list, icon_origin, scale, card_b,
                        kVersionCardAccentColor);
      } else {
        DrawVersionCard(draw_list, icon_origin, scale, card_b,
                        kVersionCardAccentColor);
        DrawVersionCard(draw_list, icon_origin, scale, card_a,
                        kVersionCardPrimaryColor);
      }
    } else if (outgoing_is_card_a) {
      DrawVersionCard(draw_list, icon_origin, scale, card_b,
                      kVersionCardAccentColor);
      DrawVersionCard(draw_list, icon_origin, scale, card_a,
                      kVersionCardPrimaryColor);
    } else {
      DrawVersionCard(draw_list, icon_origin, scale, card_a,
                      kVersionCardPrimaryColor);
      DrawVersionCard(draw_list, icon_origin, scale, card_b,
                      kVersionCardAccentColor);
    }
  } else if (version_switch_cards_swapped_) {
    DrawVersionCard(draw_list, icon_origin, scale, card_b,
                    kVersionCardAccentColor);
    DrawVersionCard(draw_list, icon_origin, scale, card_a,
                    kVersionCardPrimaryColor);
  } else {
    DrawVersionCard(draw_list, icon_origin, scale, card_a,
                    kVersionCardPrimaryColor);
    DrawVersionCard(draw_list, icon_origin, scale, card_b,
                    kVersionCardAccentColor);
  }
  draw_list->PopClipRect();
}

void FlexItemWidget::Paint() {
  if (filtered())
    return;

  CreateTextureIfNotExists();
  const SDL_Rect kBoundsToWindow = MapToWindow(bounds());
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  // Draw stretched image
  if (current_data()->image_texture) {
    draw_list->AddImage(
        reinterpret_cast<ImTextureID>(current_data()->image_texture.load()),
        ImVec2(kBoundsToWindow.x, kBoundsToWindow.y),
        ImVec2(kBoundsToWindow.x + kBoundsToWindow.w,
               kBoundsToWindow.y + kBoundsToWindow.h));
  } else {
    // Texture is not ready yet, so draw a loading spin and a rectangle
    SDL_Rect loading_widget_bounds =
        Center(SDL_Rect{kBoundsToWindow.x, kBoundsToWindow.y, kBoundsToWindow.w,
                        kBoundsToWindow.h},
               loading_widget_.CalculateCircleAABB(nullptr));
    loading_widget_.set_bounds(loading_widget_bounds);
    loading_widget_.Paint();

    draw_list->AddRect(ImVec2(kBoundsToWindow.x, kBoundsToWindow.y),
                       ImVec2(kBoundsToWindow.x + kBoundsToWindow.w,
                              kBoundsToWindow.y + kBoundsToWindow.h),
                       ImColor(255, 255, 255), 0, 0, .3f);
  }

  const bool is_selected = !parent_->empty() && parent_->IsItemSelected(this);
  if (current_data()->is_hd_edition)
    hd_edition_badge_.Paint(draw_list, kBoundsToWindow, is_selected);

  if (has_sub_items())
    PaintVersionSwitchIcon(draw_list, kBoundsToWindow, is_selected);

  // Items can be empty, because we can use filter.
  if (!parent_->empty()) {
    // Highlight selected item.
    if (is_selected) {
      int elapsed = fade_timer_.ElapsedInMilliseconds();
      int rgb = static_cast<int>(512 * elapsed / kFadeDurationInMs) % 512;
      if (rgb > 255)
        rgb = 512 - rgb;

      draw_list->AddRect(ImVec2(kBoundsToWindow.x, kBoundsToWindow.y),
                         ImVec2(kBoundsToWindow.x + kBoundsToWindow.w,
                                kBoundsToWindow.y + kBoundsToWindow.h),
                         ImColor(rgb, rgb, rgb));
    } else {
      fade_timer_.Reset();
    }
  }
}
