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

#include "nes/ppu_texture_capture.h"

#include <algorithm>

namespace kiwi {
namespace nes {

namespace {
constexpr int kScreenWidth = 256;
constexpr int kScreenHeight = 240;
}  // namespace

PPUTextureCapture::PPUTextureCapture() {
  BeginFrame();
}

PPUTextureCapture::~PPUTextureCapture() = default;

void PPUTextureCapture::SetEnabled(bool enabled, bool uses_chr_ram) {
  enabled_ = enabled;
  uses_chr_ram_ = enabled && uses_chr_ram;
  backdrop_pixels_.resize(
      enabled ? static_cast<size_t>(kScreenWidth) * kScreenHeight : 0);
  if (enabled) {
    background_tiles_.reserve(33 * 31);
    sprite_tiles_.reserve(128);
  }
  BeginFrame();
}

void PPUTextureCapture::BeginFrame() {
  background_tiles_.clear();
  sprite_tiles_.clear();
  background_command_indices_.fill(kNoTextureCommand);
  sprite_command_indices_.fill(kNoTextureCommand);
}

PPUTextureTile* PPUTextureCapture::CreateBackgroundTileCommand(
    int x,
    int y,
    Byte tile_x,
    Byte tile_y,
    Address pattern_address,
    Byte palette_slot,
    Color color,
    bool opaque) {
  const int tile_origin_x = x - tile_x;
  const int tile_origin_y = y - tile_y;
  const uint32_t tile_index = (pattern_address & 0x1ff0) / 16;
  const PPUTextureTile::Source source = uses_chr_ram_
                                            ? PPUTextureTile::Source::kChrRam
                                            : PPUTextureTile::Source::kChrRom;

  const size_t command_index = background_tiles_.size();
  PPUTextureTileCommand& command = background_tiles_.emplace_back();
  command.x = tile_origin_x;
  command.y = tile_origin_y;
  command.palette_slot = palette_slot;
  command.tile.source = source;
  command.tile.tile_index = tile_index;

  const int first_x = std::max(tile_origin_x, 0);
  const int end_x = std::min(tile_origin_x + 8, kScreenWidth);
  for (int screen_x = first_x; screen_x < end_x; ++screen_x) {
    background_command_indices_[screen_x] = command_index;
  }

  CaptureTilePixel(&command, tile_x, tile_y, color, opaque);
  return &command.tile;
}

PPUTextureTile* PPUTextureCapture::CreateSpriteTileCommand(
    int x,
    int y,
    Byte tile_x,
    Byte tile_y,
    Byte oam_index,
    Address pattern_address,
    Byte palette_slot,
    bool horizontal_mirroring,
    bool vertical_mirroring,
    bool background_priority,
    Color color,
    bool opaque) {
  const int tile_origin_x = x - tile_x;
  const int tile_origin_y = y - tile_y;
  const uint32_t tile_index = (pattern_address & 0x1ff0) / 16;
  const PPUTextureTile::Source source = uses_chr_ram_
                                            ? PPUTextureTile::Source::kChrRam
                                            : PPUTextureTile::Source::kChrRom;

  const size_t command_index = sprite_tiles_.size();
  PPUTextureTileCommand& command = sprite_tiles_.emplace_back();
  command.x = tile_origin_x;
  command.y = tile_origin_y;
  command.palette_slot = palette_slot;
  command.oam_index = oam_index;
  command.tile.source = source;
  command.tile.tile_index = tile_index;
  command.tile.sprite_palette = palette_slot;
  command.tile.horizontal_mirroring = horizontal_mirroring;
  command.tile.vertical_mirroring = vertical_mirroring;
  command.tile.background_priority = background_priority;
  sprite_command_indices_[oam_index] = command_index;

  CaptureTilePixel(&command, tile_x, tile_y, color, opaque);
  return &command.tile;
}

}  // namespace nes
}  // namespace kiwi
