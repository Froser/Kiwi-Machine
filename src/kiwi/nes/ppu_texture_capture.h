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

#ifndef NES_PPU_TEXTURE_CAPTURE_H_
#define NES_PPU_TEXTURE_CAPTURE_H_

#include <array>
#include <cstddef>
#include <vector>

#include <stdint.h>
#include "base/compiler_specific.h"
#include "nes/nes_export.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class PPU;

struct NES_EXPORT PPUTextureTile {
  enum class Source {
    kChrRom,
    kChrRam,
  };

  // Selects whether texture lookup uses a CHR-ROM index or raw CHR-RAM bytes.
  Source source = Source::kChrRom;
  // Index of the 16-byte pattern in the active CHR-ROM address space.
  uint32_t tile_index = 0;
  // Raw pattern bitplanes used as the lookup key for CHR-RAM tiles.
  std::array<Byte, 16> chr_data = {};
  // Resolved NES palette values used to match a texture rule.
  std::array<Byte, 4> palette = {};
  // Two-bit sprite palette selector used by Mesen sprite conditions.
  Byte sprite_palette = 0;
  // Whether the sprite is horizontally flipped by its OAM attributes.
  bool horizontal_mirroring = false;
  // Whether the sprite is vertically flipped by its OAM attributes.
  bool vertical_mirroring = false;
  // Whether the sprite is rendered behind opaque background pixels.
  bool background_priority = false;
};

struct NES_EXPORT PPUTextureTileCommand {
  // Tile identity, palette, and sprite attributes shared by this command.
  PPUTextureTile tile;
  // Native PPU colors used when no replacement texture rule matches.
  std::array<Color, 64> native_colors = {};
  // Marks tile-local pixels reached by this command during rasterization.
  uint64_t visible_mask = 0;
  // Marks visible pixels whose NES pattern color is nontransparent.
  uint64_t opaque_mask = 0;
  // Tile origin on screen; masks and colors use row-major 8x8 coordinates.
  int x = 0;
  int y = 0;
  // Raw two-bit palette slot used when coalescing pixels into tile commands.
  Byte palette_slot = 0;
  // Original OAM entry index used to preserve sprite priority order.
  Byte oam_index = 0;
};

struct NES_EXPORT PPUTextureScroll {
  int32_t x = 0;
  int32_t y = 0;
};

// Records format-neutral texture metadata without changing the native frame.
class NES_EXPORT PPUTextureCapture {
  friend class PPU;

  PPUTextureCapture();
  ~PPUTextureCapture();

  void SetEnabled(bool enabled, bool uses_chr_ram);
  bool enabled() const { return enabled_; }
  bool uses_chr_ram() const { return uses_chr_ram_; }
  void BeginFrame();

  // Returns a tile only when a command is created, allowing PPU to populate
  // CHR data and palette once per command.
  ALWAYS_INLINE PPUTextureTile* CaptureBackgroundTilePixel(
      int x,
      int y,
      Byte tile_x,
      Byte tile_y,
      uint32_t tile_index,
      Byte palette_slot,
      Color color,
      bool opaque) {
    const int tile_origin_x = x - tile_x;
    const int tile_origin_y = y - tile_y;
    const size_t command_index = background_command_indices_[x];
    if (command_index == kNoTextureCommand) {
      return CreateBackgroundTileCommand(x, y, tile_x, tile_y, tile_index,
                                         palette_slot, color, opaque);
    }

    PPUTextureTileCommand& command = background_tiles_[command_index];
    if (command.x != tile_origin_x || command.y != tile_origin_y ||
        command.tile.tile_index != tile_index ||
        command.palette_slot != palette_slot) {
      return CreateBackgroundTileCommand(x, y, tile_x, tile_y, tile_index,
                                         palette_slot, color, opaque);
    }

    CaptureTilePixel(&command, tile_x, tile_y, color, opaque);
    return nullptr;
  }

  ALWAYS_INLINE PPUTextureTile* CaptureSpriteTilePixel(
      int x,
      int y,
      Byte tile_x,
      Byte tile_y,
      Byte oam_index,
      uint32_t tile_index,
      Byte palette_slot,
      bool horizontal_mirroring,
      bool vertical_mirroring,
      bool background_priority,
      Color color,
      bool opaque) {
    const int tile_origin_x = x - tile_x;
    const int tile_origin_y = y - tile_y;
    const size_t command_index = sprite_command_indices_[oam_index];
    if (command_index == kNoTextureCommand) {
      return CreateSpriteTileCommand(x, y, tile_x, tile_y, oam_index,
                                     tile_index, palette_slot,
                                     horizontal_mirroring, vertical_mirroring,
                                     background_priority, color, opaque);
    }

    PPUTextureTileCommand& command = sprite_tiles_[command_index];
    if (command.x != tile_origin_x || command.y != tile_origin_y ||
        command.tile.tile_index != tile_index ||
        command.palette_slot != palette_slot ||
        command.tile.horizontal_mirroring != horizontal_mirroring ||
        command.tile.vertical_mirroring != vertical_mirroring ||
        command.tile.background_priority != background_priority) {
      return CreateSpriteTileCommand(x, y, tile_x, tile_y, oam_index,
                                     tile_index, palette_slot,
                                     horizontal_mirroring, vertical_mirroring,
                                     background_priority, color, opaque);
    }

    CaptureTilePixel(&command, tile_x, tile_y, color, opaque);
    return nullptr;
  }

  ALWAYS_INLINE void CaptureBackdropPixel(int x, int y, Color color) {
    backdrop_pixels_[static_cast<size_t>(y) * 256 + x] = color;
  }

  ALWAYS_INLINE void CaptureScroll(int y,
                                   Address data_address,
                                   Byte fine_scroll_x) {
    PPUTextureScroll& scroll = scroll_offsets_[y];
    scroll.x = ((data_address & 0x001f) << 3) | fine_scroll_x |
               ((data_address & 0x0400) ? 0x100 : 0);
    scroll.y = ((data_address & 0x03e0) >> 2) | ((data_address & 0x7000) >> 12);
    if (data_address & 0x0800) {
      scroll.y += 240;
    }
  }

  const Colors& backdrop_pixels() const { return backdrop_pixels_; }
  const std::vector<PPUTextureTileCommand>& background_tiles() const {
    return background_tiles_;
  }
  const std::vector<PPUTextureTileCommand>& sprite_tiles() const {
    return sprite_tiles_;
  }
  const std::array<PPUTextureScroll, 240>& scroll_offsets() const {
    return scroll_offsets_;
  }

  // Sentinel used when no command currently owns a column or OAM entry.
  static constexpr size_t kNoTextureCommand = static_cast<size_t>(-1);
  static ALWAYS_INLINE void CaptureTilePixel(PPUTextureTileCommand* command,
                                             Byte tile_x,
                                             Byte tile_y,
                                             Color color,
                                             bool opaque) {
    const size_t pixel_index = static_cast<size_t>(tile_y) * 8 + tile_x;
    const uint64_t pixel_mask = uint64_t{1} << pixel_index;
    command->visible_mask |= pixel_mask;
    command->native_colors[pixel_index] = color;
    if (opaque) {
      command->opaque_mask |= pixel_mask;
    } else {
      command->opaque_mask &= ~pixel_mask;
    }
  }

  PPUTextureTile* CreateBackgroundTileCommand(int x,
                                              int y,
                                              Byte tile_x,
                                              Byte tile_y,
                                              uint32_t tile_index,
                                              Byte palette_slot,
                                              Color color,
                                              bool opaque);
  PPUTextureTile* CreateSpriteTileCommand(int x,
                                          int y,
                                          Byte tile_x,
                                          Byte tile_y,
                                          Byte oam_index,
                                          uint32_t tile_index,
                                          Byte palette_slot,
                                          bool horizontal_mirroring,
                                          bool vertical_mirroring,
                                          bool background_priority,
                                          Color color,
                                          bool opaque);

  // Gates metadata collection without affecting native PPU rendering.
  bool enabled_ = false;
  // Requires tile identity to include raw CHR bytes instead of only an index.
  bool uses_chr_ram_ = false;
  // Native-resolution backdrop underneath captured tile layers.
  Colors backdrop_pixels_;
  // Coalesced background commands for the current frame.
  std::vector<PPUTextureTileCommand> background_tiles_;
  // Coalesced sprite commands for the current frame.
  std::vector<PPUTextureTileCommand> sprite_tiles_;
  // PPU scroll coordinates captured at the start of each visible scanline.
  std::array<PPUTextureScroll, 240> scroll_offsets_;
  // Latest command covering each screen column, or |kNoTextureCommand|.
  std::array<size_t, 256> background_command_indices_;
  // Latest command for each OAM entry, or |kNoTextureCommand|.
  std::array<size_t, 64> sprite_command_indices_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_PPU_TEXTURE_CAPTURE_H_
