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

#include "ui/widgets/nametable_widget.h"

#include <SDL.h>

#include "ui/window_base.h"

constexpr int kNametableWidth = 256 * 2;
constexpr int kNametableHeight = 240 * 2;

NametableWidget::NametableWidget(WindowBase* window_base,
                                 NESRuntimeID runtime_id)
    : Widget(window_base) {
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoSavedSettings;
  set_flags(window_flags);
  set_title("Nametable");
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id);
  runtime_data_->debug_port->SetNametableRenderer(this);
  set_bounds(SDL_Rect{0, 0, kNametableWidth, kNametableHeight});
}

NametableWidget::~NametableWidget() {
  SDL_assert(runtime_data_);
  runtime_data_->debug_port->SetNametableRenderer(nullptr);

  if (screen_texture_)
    SDL_DestroyTexture(screen_texture_);
}

void NametableWidget::Paint() {
  kiwi::nes::DebugPort* debug_port = runtime_data_->debug_port.get();

  // Write buffer to the texture, and render.
  if (!screen_buffer_.empty()) {
    int result =
        SDL_UpdateTexture(screen_texture_, nullptr, screen_buffer_.data(),
                          kNametableWidth * sizeof(screen_buffer_[0]));
    SDL_assert(result == 0);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    SDL_Rect render_bounds = bounds();
    ImVec2 work_pos = ImGui::GetMainViewport()->WorkPos;
    ImVec2 window_pos = ImGui::GetWindowPos();
    draw_list->AddImage(
        screen_texture_,
        ImVec2(work_pos.x + window_pos.x + render_bounds.x,
               work_pos.y + window_pos.y + render_bounds.y),
        ImVec2(work_pos.x + window_pos.x + render_bounds.x + render_bounds.w,
               work_pos.y + window_pos.y + render_bounds.y + render_bounds.h));
  }
}

void NametableWidget::Render(int width, int height, const Buffer& buffer) {
  // Create texture to show nametable
  if (!screen_texture_) {
    // SDL_assert(frame_->width() > 0 && frame_->height() > 0);
    screen_texture_ =
        SDL_CreateTexture(window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
                          SDL_TEXTUREACCESS_STREAMING, width, height);
  }

  screen_buffer_ = buffer;
}

bool NametableWidget::NeedRender() {
  return visible();
}