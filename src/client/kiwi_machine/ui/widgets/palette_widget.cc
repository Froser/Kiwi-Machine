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

#include "ui/widgets/palette_widget.h"

#include <imgui.h>

namespace {
std::string ToHex(int n) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(2) << n;
  return ss.str();
}
}  // namespace

PaletteWidget::PaletteWidget(WindowBase* window_base,
                             kiwi::nes::DebugPort* debug_port)
    : Widget(window_base), debug_port_(debug_port) {
  set_flags(ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
  SDL_assert(debug_port);
  set_title("Palette");
}

PaletteWidget::~PaletteWidget() = default;

void PaletteWidget::Paint() {
  kiwi::nes::Palette* palette = debug_port_->GetPPUContext().palette;
  // A palette's index is from 0 to 63.
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 16; ++col) {
      int index = col + row * 16;
      kiwi::nes::Color color = palette->GetColorBGRA(index);
      float colors[4] = {
          ((color >> 16) & 0xff) / 255.f, ((color >> 8) & 0xff) / 255.f,
          ((color >> 0) & 0xff) / 255.f, ((color >> 24) & 0xff) / 255.f};
      ImGui::ColorEdit4(
          "", colors,
          ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions);
      // 16 colors in a row.
      if ((index + 1) % 16 != 0) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 0));
        ImGui::SameLine();
        ImGui::PopStyleVar();
      }
    }

    for (int col = 0; col < 16; ++col) {
      int index = col + row * 16;
      ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s",
                         ("$" + ToHex(index)).c_str());
      if ((index + 1) % 16 != 0)
        ImGui::SameLine();
    }
  }
}
