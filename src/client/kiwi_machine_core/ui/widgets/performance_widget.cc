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

#include "ui/widgets/performance_widget.h"

#include <imgui.h>

constexpr ImVec2 kGraphSize(300, 150);

PerformanceWidget::PerformanceWidget(WindowBase* window_base,
                                     scoped_refptr<NESFrame> frame,
                                     DebugPort* debug_port)
    : Widget(window_base), frame_(frame), debug_port_(debug_port) {
  SDL_assert(frame_);
  frame_->AddObserver(this);
  debug_port_->AddObserver(this);
  Application::Get()->AddObserver(this);
  set_flags(ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
  set_title("Performance");
}

PerformanceWidget::~PerformanceWidget() {
  SDL_assert(frame_);
  frame_->RemoveObserver(this);
  debug_port_->RemoveObserver(this);
  Application::Get()->RemoveObserver(this);
}

void PerformanceWidget::Paint() {
  if (ImGui::BeginTabBar("Performance Tab", ImGuiTabBarFlags_None)) {
    if (ImGui::BeginTabItem("Frame Rate")) {
      ImGui::PlotLines("", app_frame_since_last_.samples, kSampleCount,
                       app_frame_since_last_.index,
                       "Application Frame Rate (fps)", 0.f, 120.f, kGraphSize);

      ImGui::PlotLines("", nes_frame_generate_.samples, kSampleCount,
                       nes_frame_generate_.index,
                       "NES Frame Generate Rate (fps)", 0.f, 120.f, kGraphSize);

      ImGui::PlotLines("", nes_frame_present_.samples, kSampleCount,
                       nes_frame_present_.index, "NES Frame Present Rate (fps)",
                       0.f, 120.f, kGraphSize);

      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("CPU & PPU costs")) {
      ImGui::PlotLines(
          "", nes_cpu_ppu_total_ms_costs_per_frame_.samples, kSampleCount,
          nes_cpu_ppu_total_ms_costs_per_frame_.index,
          "NES CPU & PPU total costs per frame (ms)", 0.f, 40.f, kGraphSize);

      ImGui::PlotLines("", nes_cpu_ms_costs_per_frame_.samples, kSampleCount,
                       nes_cpu_ms_costs_per_frame_.index,
                       "NES CPU costs per frame (ms)", 0.f, 20.f, kGraphSize);

      ImGui::PlotLines("", nes_ppu_ms_costs_per_frame_.samples, kSampleCount,
                       nes_ppu_ms_costs_per_frame_.index,
                       "NES PPU costs per frame (ms)", 0.f, 20.f, kGraphSize);

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

void PerformanceWidget::Update(Plot* plot, float value) {
  (plot->samples)[plot->index] = value;
  plot->index = (plot->index + 1) % kSampleCount;
}

void PerformanceWidget::OnShouldRender(int since_last_frame_ms) {
  Update(&nes_frame_present_, 1000.f / since_last_frame_ms);
}

void PerformanceWidget::OnPreRender(int since_last_frame_ms) {
  Update(&app_frame_since_last_, 1000.f / since_last_frame_ms);
}

void PerformanceWidget::OnFrameEnd(int since_last_frame_duration_ms,
                                   int cpu_last_frame_duration_ms,
                                   int ppu_last_frame_duration_ms) {
  Update(&nes_frame_generate_, 1000.f / since_last_frame_duration_ms);
  Update(&nes_cpu_ppu_total_ms_costs_per_frame_,
         cpu_last_frame_duration_ms + ppu_last_frame_duration_ms);
  Update(&nes_cpu_ms_costs_per_frame_, cpu_last_frame_duration_ms);
  Update(&nes_ppu_ms_costs_per_frame_, ppu_last_frame_duration_ms);
}