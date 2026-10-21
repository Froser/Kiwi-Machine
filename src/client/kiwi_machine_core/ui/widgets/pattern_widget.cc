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

#include "ui/widgets/pattern_widget.h"

#include <imgui.h>

#include <iterator>

#include "ui/main_window.h"
#include "ui/window_base.h"

namespace {
// https://24ways.org/2010/calculating-color-contrast
bool IsColorTooLight(ImColor color) {
  constexpr int kFactor = 255;
  return (kFactor *
          (color.Value.x * 299 + color.Value.y * 587 + color.Value.z * 114) /
          1000) >= 128;
}

void DrawPaletteIndex(ImDrawList* draw_list,
                      kiwi::nes::Byte index,
                      ImColor palette_color,
                      int palette_x,
                      int palette_y,
                      int palette_width,
                      int palette_height) {
  std::stringstream ss;
  ss << std::hex << std::setfill('0') << std::setw(2)
     << static_cast<int>(index);
  std::string str = ss.str();

  ImU32 pen_color =
      IsColorTooLight(palette_color) ? IM_COL32_BLACK : IM_COL32_WHITE;

  constexpr int kFontDefaultPointSize = 12;

  ImVec2 font_size = ImGui::CalcTextSize(str.c_str());
  draw_list->AddText(ImVec2(palette_x + (palette_width - font_size.x) / 2,
                            palette_y + (palette_height - font_size.y) / 2),
                     pen_color, str.c_str());
}

constexpr int kPatternTableWidth = 256;
constexpr int kPatternTableHeight = 128;
constexpr int kPaletteTileSize =
    32;  // A palette is presented as a 32*32 square.
constexpr int kPatternTableCount = 8;
}  // namespace

PatternWidget::PatternWidget(MainWindow* main_window)
    : Widget(main_window), main_window_(main_window) {
  set_flags(ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
  set_title("Pattern");
  set_bounds(
      SDL_Rect{0, 0, 430, kPatternTableCount * (kPatternTableHeight + 10)});
}

PatternWidget::~PatternWidget() {
  for (auto* pattern_table : pattern_tables_) {
    if (pattern_table) {
      SDL_DestroyTexture(pattern_table);
    }
  }
}

void PatternWidget::Paint() {
  DebugPort* debug_port = main_window_->debug_port();
  if (!debug_port)
    return;
  if (first_paint_) {
    for (auto& pattern_table : pattern_tables_) {
      SDL_assert(!pattern_table);
      pattern_table = SDL_CreateTexture(
          window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
          SDL_TEXTUREACCESS_STREAMING, kPatternTableWidth, kPatternTableHeight);
      if (pattern_table)
        SDL_SetTextureScaleMode(pattern_table, SDL_ScaleModeNearest);
    }

    first_paint_ = false;
  }
  kiwi::nes::Palette* palette = debug_port->GetPPUContext().palette;
  assert(palette);

  for (int i = 0; i < kPatternTableCount; ++i) {
    kiwi::nes::Colors bgra =
        debug_port->GetPatternTableBGRA(static_cast<kiwi::nes::PaletteName>(i));
    if (bgra.empty())
      break;
    if (!pattern_tables_[i])
      continue;

    SDL_UpdateTexture(pattern_tables_[i], nullptr, bgra.data(),
                      kPatternTableWidth * sizeof(bgra[0]));
    ImGui::PushID(i);
    ImGui::Image(reinterpret_cast<ImTextureID>(pattern_tables_[i]),
                 ImVec2(kPatternTableWidth, kPatternTableHeight));
    ImGui::SameLine();
    ImGui::BeginGroup();
    kiwi::nes::Byte indices[4];
    debug_port->GetPaletteIndices(static_cast<kiwi::nes::PaletteName>(i),
                                  indices);
    for (size_t palette_index = 0; palette_index < std::size(indices);
         ++palette_index) {
      const kiwi::nes::Byte index = indices[palette_index];
      const kiwi::nes::Color palette_bgra = palette->GetColorBGRA(index);
      const ImU32 color =
          IM_COL32((palette_bgra >> 16) & 0xff, (palette_bgra >> 8) & 0xff,
                   palette_bgra & 0xff, (palette_bgra >> 24) & 0xff);
      ImGui::PushID(static_cast<int>(palette_index));
      const ImVec2 palette_position = ImGui::GetCursorScreenPos();
      ImGui::InvisibleButton("##palette",
                             ImVec2(kPaletteTileSize, kPaletteTileSize));
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      draw_list->AddRectFilled(palette_position,
                               ImVec2(palette_position.x + kPaletteTileSize,
                                      palette_position.y + kPaletteTileSize),
                               color);
      DrawPaletteIndex(draw_list, index, ImColor(color),
                       static_cast<int>(palette_position.x),
                       static_cast<int>(palette_position.y), kPaletteTileSize,
                       kPaletteTileSize);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Palette $%02X", index);
      ImGui::PopID();
      if (palette_index + 1 < std::size(indices))
        ImGui::SameLine();
    }
    ImGui::EndGroup();
    ImGui::PopID();
  }
}
