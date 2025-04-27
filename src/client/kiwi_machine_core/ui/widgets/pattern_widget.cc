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

SDL_Rect MapToWindow(const SDL_Rect& rect) {
  ImVec2 window_pos = ImGui::GetWindowPos();
  return {static_cast<int>(rect.x + window_pos.x),
          static_cast<int>(rect.y + window_pos.y), rect.w, rect.h};
}

// Save all 8 pattern tables and palettes in a big canvas.
constexpr int kTopMargin = 20;
constexpr int kPatternTableWidth = 256;
constexpr int kPatternTableHeight = 128;
constexpr int kMargin = 10;
constexpr int kSpacing = 10;
constexpr int kPaletteTileSize =
    32;  // A palette is presented as a 32*32 square.
constexpr int kPaletteSpacingBetweenPatternTable = 32;
constexpr int kHeight = kMargin * 2 + 8 * kPatternTableHeight + 7 * kSpacing;
constexpr int kWidth = kMargin * 2 + kPatternTableWidth +
                       kPaletteSpacingBetweenPatternTable +
                       kPaletteTileSize * 4;
}  // namespace

PatternWidget::PatternWidget(WindowBase* window_base,
                             kiwi::nes::DebugPort* debug_port)
    : Widget(window_base), debug_port_(debug_port) {
  set_flags(ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
  SDL_assert(debug_port);
  set_title("Pattern");
  set_bounds(SDL_Rect{0, 0, kWidth, kHeight + kTopMargin});
}

PatternWidget::~PatternWidget() {
  for (auto* pattern_table : pattern_tables_) {
    if (pattern_table) {
      SDL_DestroyTexture(pattern_table);
    }
  }
}

void PatternWidget::Paint() {
  if (first_paint_) {
    for (auto& pattern_table : pattern_tables_) {
      SDL_assert(!pattern_table);
      pattern_table = SDL_CreateTexture(
          window()->renderer(), SDL_PIXELFORMAT_ARGB8888,
          SDL_TEXTUREACCESS_STREAMING, kPatternTableWidth, kPatternTableHeight);
    }

    first_paint_ = false;
  }
  ImDrawList* draw_list = ImGui::GetWindowDrawList();

  int offset_y = kMargin + kTopMargin;
  kiwi::nes::Palette* palette = debug_port_->GetPPUContext().palette;
  assert(palette);

  // This is not a good way to get window's position. Consider move it to
  // window's base infra.
  ImVec2 window_pos = ImGui::GetWindowPos();

  for (int i = 0; i < 8; ++i) {
    // Draw each pattern table.
    kiwi::nes::Colors bgra = debug_port_->GetPatternTableBGRA(
        static_cast<kiwi::nes::PaletteName>(i));

    // If ROMs is not loaded, color buffer will be empty.
    if (bgra.empty())
      break;

    {
      SDL_UpdateTexture(pattern_tables_[i], nullptr, bgra.data(),
                        kPatternTableWidth * sizeof(bgra[0]));
      SDL_Rect pattern_bounds =
          MapToWindow({static_cast<int>(window_pos.x) + kMargin,
                       static_cast<int>(window_pos.y) + offset_y,
                       kPatternTableWidth, kPatternTableHeight});
      draw_list->AddImage(reinterpret_cast<ImTextureID>(pattern_tables_[i]),
                          ImVec2(pattern_bounds.x, pattern_bounds.y),
                          ImVec2(pattern_bounds.x + pattern_bounds.w,
                                 pattern_bounds.y + pattern_bounds.h));
    }

    // Draw each palette.
    int palette_x =
        kMargin + kPatternTableWidth + kPaletteSpacingBetweenPatternTable;
    int palette_y = offset_y + (kPatternTableHeight - kPaletteTileSize) / 2;
    kiwi::nes::Byte indices[4];
    debug_port_->GetPaletteIndices(static_cast<kiwi::nes::PaletteName>(i),
                                   indices);
    for (unsigned char index : indices) {
      {
        kiwi::nes::Color palette_bgra = palette->GetColorBGRA(index);
        SDL_Rect palette_bounds =
            MapToWindow({static_cast<int>(window_pos.x) + palette_x,
                         static_cast<int>(window_pos.y) + palette_y,
                         kPaletteTileSize, kPaletteTileSize});
        ImU32 color =
            IM_COL32((palette_bgra >> 16) & 0xff, (palette_bgra >> 8) & 0xff,
                     palette_bgra & 0xff, (palette_bgra >> 24) & 0xff);
        draw_list->AddRectFilled(ImVec2(palette_bounds.x, palette_bounds.y),
                                 ImVec2(palette_bounds.x + palette_bounds.w,
                                        palette_bounds.y + palette_bounds.h),
                                 color);

        DrawPaletteIndex(draw_list, index, ImColor(color), palette_bounds.x,
                         palette_bounds.y, kPaletteTileSize, kPaletteTileSize);
      }

      palette_x += kPaletteTileSize;
    }

    offset_y += kPatternTableHeight + kSpacing;
  }
}
