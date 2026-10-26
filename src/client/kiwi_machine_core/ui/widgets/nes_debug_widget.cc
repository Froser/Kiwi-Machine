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

#include "ui/widgets/nes_debug_widget.h"

#include <array>
#include <utility>

#include "debug/debug_port.h"
#include "models/nes/frame.h"
#include "ui/main_window.h"
#include "ui/widgets/disassembly_widget.h"
#include "ui/widgets/memory_widget.h"
#include "ui/widgets/nametable_widget.h"
#include "ui/widgets/palette_widget.h"
#include "ui/widgets/pattern_widget.h"
#include "ui/widgets/performance_widget.h"

namespace {

struct AudioChannelControl {
  const char* name;
  kiwi::nes::AudioChannel mask;
};

constexpr std::array<AudioChannelControl, 5> kAudioChannels = {{
    {"Square 1", kiwi::nes::kSquare_1},
    {"Square 2", kiwi::nes::kSquare_2},
    {"Triangle", kiwi::nes::kTriangle},
    {"Noise", kiwi::nes::kNoise},
    {"DMC", kiwi::nes::kDMC},
}};

}  // namespace

NESDebugWidget::NESDebugWidget(MainWindow* main_window,
                               scoped_refptr<NESFrame> frame,
                               kiwi::base::RepeatingClosure on_toggle_pause,
                               kiwi::base::RepeatingCallback<bool()> is_pause)
    : Widget(main_window),
      main_window_(main_window),
      on_toggle_pause_(std::move(on_toggle_pause)),
      is_pause_(std::move(is_pause)),
      palette_widget_(std::make_unique<PaletteWidget>(main_window)),
      pattern_widget_(std::make_unique<PatternWidget>(main_window)),
      performance_widget_(
          std::make_unique<PerformanceWidget>(main_window, std::move(frame))),
      memory_widget_(std::make_unique<MemoryWidget>(main_window,
                                                    on_toggle_pause_,
                                                    is_pause_)),
      disassembly_widget_(std::make_unique<DisassemblyWidget>(main_window,
                                                              on_toggle_pause_,
                                                              is_pause_)),
      nametable_widget_(std::make_unique<NametableWidget>(main_window)) {
  set_flags(ImGuiWindowFlags_NoSavedSettings);
  set_bounds(SDL_Rect{80, 30, 1040, 660});
  set_title("NES Debugger");

  // These views are painted by this aggregate widget. Their visibility still
  // gates snapshot refreshes in the legacy view implementations.
  memory_widget_->set_visible(true);
  disassembly_widget_->set_visible(true);
}

NESDebugWidget::~NESDebugWidget() = default;

void NESDebugWidget::SetDebugPort(DebugPort* debug_port) {
  debug_port_ = debug_port;
  performance_widget_->SetDebugPort(debug_port);
  nametable_widget_->SetDebugPort(debug_port);
}

void NESDebugWidget::Refresh() {
  if (!visible() || !debug_port_ || !is_pause_.Run())
    return;
  memory_widget_->UpdateMemory();
  disassembly_widget_->UpdateDisassembly();
  if (nametable_tab_visible_)
    nametable_widget_->Refresh();
}

void NESDebugWidget::OnWindowPreRender() {
  nametable_widget_->SetRenderEnabled(false);
}

void NESDebugWidget::Paint() {
  if (!debug_port_ || !main_window_->runtime()->emulator()->HasROM()) {
    nametable_tab_visible_ = false;
    ImGui::TextUnformatted("Load an NES ROM to use the debugger.");
    return;
  }

  PaintToolbar();
  ImGui::Separator();

  const bool was_nametable_visible = nametable_tab_visible_;
  nametable_tab_visible_ = false;
  if (ImGui::BeginTabBar("NES Debugger Tabs")) {
    if (ImGui::BeginTabItem("CPU")) {
      disassembly_widget_->Paint();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Memory")) {
      memory_widget_->Paint();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Graphics")) {
      PaintGraphicsTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Audio")) {
      PaintAudioTab();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Performance")) {
      performance_widget_->Paint();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  if (nametable_tab_visible_ && !was_nametable_visible && is_pause_.Run())
    nametable_widget_->Refresh();
}

void NESDebugWidget::PaintToolbar() {
  const bool paused = is_pause_.Run();
  if (ImGui::Button(paused ? "Resume" : "Pause"))
    on_toggle_pause_.Run();

  ImGui::SameLine();
  ImGui::BeginDisabled(!paused);
  if (ImGui::Button("Refresh"))
    Refresh();
  ImGui::EndDisabled();

  ImGui::SameLine();
  bool render_paused = debug_port_->render_paused();
  if (ImGui::Checkbox("Pause render", &render_paused))
    debug_port_->set_render_paused(render_paused);

  ImGui::SameLine();
  ImGui::TextUnformatted(paused ? "Paused" : "Running");
}

void NESDebugWidget::PaintGraphicsTab() {
  if (ImGui::BeginTabBar("NES Graphics Views")) {
    if (ImGui::BeginTabItem("Palette")) {
      palette_widget_->Paint();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Patterns")) {
      ImGui::BeginChild("NES Patterns", ImVec2(0, 0), false,
                        ImGuiWindowFlags_HorizontalScrollbar);
      pattern_widget_->Paint();
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Nametable")) {
      nametable_tab_visible_ = true;
      nametable_widget_->SetRenderEnabled(true);
      ImGui::BeginChild("NES Nametable", ImVec2(0, 0), false,
                        ImGuiWindowFlags_HorizontalScrollbar);
      nametable_widget_->Paint();
      ImGui::EndChild();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void NESDebugWidget::PaintAudioTab() {
  int masks = debug_port_->GetAudioChannelMasks();
  for (const AudioChannelControl& channel : kAudioChannels) {
    bool enabled = (masks & channel.mask) != 0;
    if (ImGui::Checkbox(channel.name, &enabled)) {
      masks ^= channel.mask;
      debug_port_->SetAudioChannelMasks(masks);
    }
  }
}
