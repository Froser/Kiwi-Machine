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

#include "debug/debug_port.h"
#include "emulator/nes/ppu_observer.h"
#include "ui/main_window.h"
#include "ui/window_base.h"

constexpr int kNametableWidth = 256 * 2;
constexpr int kNametableHeight = 240 * 2;

NametableWidget::NametableWidget(MainWindow* main_window)
    : Widget(main_window) {
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoSavedSettings;
  set_flags(window_flags);
  set_title("Nametable");
  set_bounds(SDL_Rect{0, 0, kNametableWidth, kNametableHeight});
}

NametableWidget::~NametableWidget() {
  SetDebugPort(nullptr);

  if (screen_texture_)
    SDL_DestroyTexture(screen_texture_);
}

void NametableWidget::SetDebugPort(DebugPort* debug_port) {
  if (debug_port_) {
    debug_port_->SetNametableRenderer(nullptr);
  }
  debug_port_ = debug_port;
  if (debug_port_) {
    debug_port_->SetNametableRenderer(this);
  }
}

void NametableWidget::Paint() {
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
        reinterpret_cast<ImTextureID>(screen_texture_),
        ImVec2(work_pos.x + window_pos.x + render_bounds.x,
               work_pos.y + window_pos.y + render_bounds.y),
        ImVec2(work_pos.x + window_pos.x + render_bounds.x + render_bounds.w,
               work_pos.y + window_pos.y + render_bounds.y + render_bounds.h));
  }
}

void NametableWidget::Render(const kiwi::nes::PPUFrameData& frame) {
  if (!frame.native_pixels) {
    return;
  }

  // Create texture to show nametable
  if (!screen_texture_) {
    // SDL_assert(frame_->width() > 0 && frame_->height() > 0);
    screen_texture_ = SDL_CreateTexture(
        window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, frame.width, frame.height);
  }

  screen_buffer_ = *frame.native_pixels;
}

bool NametableWidget::NeedRender() {
  return visible();
}
