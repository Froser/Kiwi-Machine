// Copyright (C) 2026 Yisi Yu
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

#include "ui/widgets/in_game_menu.h"

#include <imgui.h>

#include <algorithm>
#include <utility>

#include "ui/widgets/canvas.h"
#include "ui/widgets/in_game_menu_internal.h"
#include "ui/window_base.h"

namespace {

using namespace in_game_menu_internal;

constexpr ImU32 kQuickLoadBackdropColor = IM_COL32(3, 7, 3, 36);
constexpr ImU32 kQuickLoadPanelColor = IM_COL32(5, 7, 6, 178);
constexpr ImU32 kQuickLoadCardColor = IM_COL32(16, 19, 17, 188);
constexpr ImU32 kQuickLoadBorderColor = IM_COL32(245, 247, 245, 120);

}  // namespace

void InGameMenu::ShowQuickLoad() {
  quick_load_mode_ = true;
  page_ = Page::kStateBrowser;
  focus_.menu_item = MenuItem::kLoadAutoSave;
  focus_.area = FocusArea::kStateSelector;
  focus_.confirm_action = false;
  has_layout_ = false;
  current_saved_states_count_ = 0;
  current_auto_states_count_ = 0;
  saved_state_count_ready_ = false;
  auto_save_count_ready_ = false;
  which_autosave_state_slot_ = 0;
  CancelStatePreviewRequest();
  ResetQuickLoadPreviews();
  RefreshStateAvailability();
  RefreshQuickLoadPreviews();
  set_visible(true);
}

void InGameMenu::LayoutQuickLoad(FrameLayout& layout) {
  SDL_Rect inner = InsetRect(layout.detail, layout.padding);
  const float gap = std::max(10.f, layout.padding * .75f);
  const float title_height = std::max(28.f, layout.font_height * 1.4f);
  constexpr float kSelectionMarkerHeight = 14.f;
  const float preview_aspect =
      static_cast<float>(Canvas::kNESFrameDefaultWidth) /
      static_cast<float>(Canvas::kNESFrameDefaultHeight);
  const float available_preview_width =
      std::max(1.f, inner.w - gap * (kQuickLoadVisibleSlotCount - 1));
  const float maximum_preview_height =
      std::max(1.f, inner.h - title_height - gap - kSelectionMarkerHeight);
  const float preview_width =
      std::min(available_preview_width / kQuickLoadVisibleSlotCount,
               maximum_preview_height * preview_aspect);
  const float preview_height = preview_width / preview_aspect;
  const float row_width = preview_width * kQuickLoadVisibleSlotCount +
                          gap * (kQuickLoadVisibleSlotCount - 1);
  const float row_x = inner.x + std::max(0.f, (inner.w - row_width) / 2.f);
  const float preview_y = inner.y + title_height + gap;

  layout.quick_load_title = MakeRect(inner.x, inner.y, inner.w, title_height);
  float preview_x = row_x;
  for (SDL_Rect& preview : layout.quick_load_previews) {
    preview = MakeRect(preview_x, preview_y, preview_width, preview_height);
    preview_x += preview_width + gap;
  }
}

bool InGameMenu::DrawQuickLoadBackground(const FrameLayout& layout) {
  if (!quick_load_mode_)
    return false;

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 window_pos = ImGui::GetWindowPos();
  ImVec2 window_size = ImGui::GetWindowSize();
  draw_list->AddRectFilled(
      window_pos,
      ImVec2(window_pos.x + window_size.x, window_pos.y + window_size.y),
      kQuickLoadBackdropColor);
  draw_list->AddRectFilled(RectMin(layout.panel), RectMax(layout.panel),
                           kQuickLoadPanelColor);
  draw_list->AddLine(RectMin(layout.panel),
                     ImVec2(RectRight(layout.panel), layout.panel.y),
                     kStrongBorderColor);
  return true;
}

void InGameMenu::DrawQuickLoad(const FrameText& text,
                               const FrameLayout& layout) {
  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  DrawTextInRect(text.state_title, layout.quick_load_title,
                 GetSecondaryFontSize(layout.font_size), kTextColor, 0.f, true,
                 false);

  for (size_t i = 0; i < kQuickLoadVisibleSlotCount; ++i) {
    const int slot = GetQuickLoadSlot(i);
    const QuickLoadPreview* preview =
        slot >= 0 ? &quick_load_previews_[slot] : nullptr;
    const SDL_Rect& rect = layout.quick_load_previews[i];
    const bool selected = i == kQuickLoadSelectedSlotIndex;

    if (!preview)
      continue;
    if (preview->status == StatePreview::Status::kLoading) {
      DrawSpinner(rect);
      continue;
    }
    if (preview->status != StatePreview::Status::kReady ||
        !preview->has_thumbnail || !preview->texture) {
      continue;
    }

    draw_list->AddRectFilled(RectMin(rect), RectMax(rect), kQuickLoadCardColor,
                             kCornerRadius);
    draw_list->AddImageRounded(reinterpret_cast<ImTextureID>(preview->texture),
                               RectMin(rect), RectMax(rect), ImVec2(0.f, 0.f),
                               ImVec2(1.f, 1.f), IM_COL32_WHITE, kCornerRadius);
    draw_list->AddRect(RectMin(rect), RectMax(rect),
                       selected ? kTextColor : kQuickLoadBorderColor,
                       kCornerRadius, 0, selected ? 3.f : 1.f);
    if (selected) {
      const float marker_width = 12.f;
      const float marker_y = RectBottom(rect) + 7.f;
      draw_list->AddTriangleFilled(
          ImVec2(rect.x + rect.w / 2.f - marker_width / 2.f, marker_y),
          ImVec2(rect.x + rect.w / 2.f + marker_width / 2.f, marker_y),
          ImVec2(rect.x + rect.w / 2.f, marker_y + marker_width / 2.f),
          kTextColor);
    }
  }
}

void InGameMenu::ResetQuickLoadPreviews() {
  ++quick_load_preview_request_id_;
  for (QuickLoadPreview& preview : quick_load_previews_) {
    preview.status = StatePreview::Status::kEmpty;
    preview.requested = false;
    preview.has_thumbnail = false;
    preview.timestamp = 0;
  }
}

void InGameMenu::RefreshQuickLoadPreviews() {
  if (!auto_save_count_ready_)
    return;

  const kiwi::nes::RomData* rom_data = runtime_data_->emulator->GetRomData();
  if (!rom_data)
    return;

  const uint64_t request_id = quick_load_preview_request_id_;
  for (size_t i = 0; i < kQuickLoadVisibleSlotCount; ++i) {
    const int slot = GetQuickLoadSlot(i);
    if (slot < 0)
      continue;

    QuickLoadPreview& preview = quick_load_previews_[slot];
    if (preview.requested)
      continue;

    preview.requested = true;
    preview.status = StatePreview::Status::kLoading;
    std::weak_ptr<int> weak_lifetime = lifetime_token_;
    runtime_data_->GetAutoSavedState(
        rom_data->crc, slot,
        kiwi::base::BindOnce(&InGameMenu::DispatchQuickLoadPreviewResult,
                             std::move(weak_lifetime), this, request_id, slot));
  }
}

void InGameMenu::DispatchQuickLoadPreviewResult(
    std::weak_ptr<int> weak_lifetime,
    InGameMenu* menu,
    uint64_t request_id,
    int slot,
    const NESRuntime::Data::StateResult& result) {
  if (!weak_lifetime.expired())
    menu->OnGotQuickLoadPreview(request_id, slot, result);
}

void InGameMenu::OnGotQuickLoadPreview(
    uint64_t request_id,
    int slot,
    const NESRuntime::Data::StateResult& state_result) {
  if (request_id != quick_load_preview_request_id_ || slot < 0 ||
      slot >= NESRuntime::Data::MaxAutoSaveStates) {
    return;
  }

  QuickLoadPreview& preview = quick_load_previews_[slot];
  preview.status = StatePreview::Status::kEmpty;
  preview.has_thumbnail = false;
  preview.timestamp = 0;
  if (!state_result.success || state_result.thumbnail_data.empty())
    return;

  preview.status = StatePreview::Status::kReady;
  preview.timestamp = state_result.slot_or_timestamp;
  if (!preview.texture) {
    preview.texture = SDL_CreateTexture(
        window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, Canvas::kNESFrameDefaultWidth,
        Canvas::kNESFrameDefaultHeight);
  }
  if (!preview.texture)
    return;

  constexpr int kColorComponents = 4;
  if (SDL_UpdateTexture(
          preview.texture, nullptr, state_result.thumbnail_data.data(),
          Canvas::kNESFrameDefaultWidth * kColorComponents *
              sizeof(state_result.thumbnail_data.data()[0])) == 0) {
    preview.has_thumbnail = true;
  }
}

int InGameMenu::GetQuickLoadSlot(size_t visible_index) const {
  if (!auto_save_count_ready_ || current_auto_states_count_ <= 0)
    return -1;

  const int offset = static_cast<int>(kQuickLoadSelectedSlotIndex) -
                     static_cast<int>(visible_index);
  const int slot = which_autosave_state_slot_ + offset;
  return slot >= 0 && slot < current_auto_states_count_ ? slot : -1;
}

int InGameMenu::GetQuickLoadTimestamp() const {
  if (which_autosave_state_slot_ < 0 ||
      which_autosave_state_slot_ >= NESRuntime::Data::MaxAutoSaveStates) {
    return 0;
  }
  return quick_load_previews_[which_autosave_state_slot_].timestamp;
}
