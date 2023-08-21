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

#include "ui/widgets/frame_rate_widget.h"

#include <imgui.h>

constexpr ImVec2 kGraphSize(300, 150);

FrameRateWidget::FrameRateWidget(WindowBase* window_base,
                                 scoped_refptr<NESFrame> frame,
                                 DebugPort* debug_port)
    : Widget(window_base), frame_(frame), debug_port_(debug_port) {
  SDL_assert(frame_);
  frame_->AddObserver(this);
  debug_port_->AddObserver(this);
  Application::Get()->AddObserver(this);
  set_flags(ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
  set_title("Frame rate");
}

FrameRateWidget::~FrameRateWidget() {
  SDL_assert(frame_);
  frame_->RemoveObserver(this);
  debug_port_->RemoveObserver(this);
  Application::Get()->RemoveObserver(this);
}

void FrameRateWidget::Paint() {
  ImGui::PlotLines("", app_frame_since_last_.samples, kSampleCount,
                   app_frame_since_last_.index, "Application Frame Rate (fps)",
                   0.f, 120.f, kGraphSize);

  ImGui::PlotLines("", nes_frame_generate_.samples, kSampleCount,
                   nes_frame_generate_.index, "NES Frame Generate Rate (fps)",
                   0.f, 120.f, kGraphSize);

  ImGui::PlotLines("", nes_frame_present_.samples, kSampleCount,
                   nes_frame_present_.index, "NES Frame Present Rate (fps)",
                   0.f, 120.f, kGraphSize);
}

void FrameRateWidget::Update(Plot* plot, float value) {
  (plot->samples)[plot->index] = value;
  plot->index = (plot->index + 1) % kSampleCount;
}

void FrameRateWidget::OnShouldRender(int since_last_frame_ms) {
  Update(&nes_frame_present_, 1000.f / since_last_frame_ms);
}

void FrameRateWidget::OnPreRender(int since_last_frame_ms) {
  Update(&app_frame_since_last_, 1000.f / since_last_frame_ms);
}

void FrameRateWidget::OnFrameEnd(int since_last_frame_end_ms) {
  Update(&nes_frame_generate_, 1000.f / since_last_frame_end_ms);
}