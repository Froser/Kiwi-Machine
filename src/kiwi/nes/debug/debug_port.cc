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

#include "nes/debug/debug_port.h"

#include "base/check.h"
#include "nes/emulator.h"
#include "nes/palette.h"
#include "nes/registers.h"

// Convert left pattern table's position to index of the vector
#define LEFT(row, col) (row) * 256 + (col)
#define RIGHT(row, col) (row) * 256 + (128 + (col))
#define BIT(a, n) (((a) >> (n)) & 1)

namespace kiwi {
namespace nes {
constexpr int16_t kAttributeTableSize = 0x40;
constexpr int kTileSize = 8;
constexpr int kPatternTableRows = 128;
constexpr int kOnePatternTablePixelsPerLine = 128;
constexpr int kTwoPatternTablePixelsPerLine = kOnePatternTablePixelsPerLine * 2;
constexpr int kNametableWidth = 256;
constexpr int kNametableHeight = 240;

Sprite::Sprite() = default;
Sprite::~Sprite() = default;
Sprite::Sprite(const Sprite& sprite) {
  bgra = sprite.bgra;
  position = sprite.position;
  is_8x8 = sprite.is_8x8;
}

Sprite::Sprite(Sprite&& sprite) {
  bgra = std::move(sprite.bgra);
  position = std::move(sprite.position);
  is_8x8 = std::move(sprite.is_8x8);
}

DebugPort::DebugPort(Emulator* emulator) : emulator_(emulator) {
  DCHECK(emulator_);
  main_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}
DebugPort::~DebugPort() = default;

void DebugPort::OnNametableRenderReady() {
  if (nametable_render_device_ && nametable_render_device_->NeedRender()) {
    kiwi::nes::Colors nametable_bgra = DebugPort::GetNametableBGRA();
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(
                       [](IODevices::RenderDevice* nametable_render_device,
                          const kiwi::nes::Colors& bgra) {
                         nametable_render_device->Render(512, 480, bgra);
                       },
                       base::Unretained(nametable_render_device_),
                       std::move(nametable_bgra)));
  }
}

PPUContext DebugPort::GetPPUContext() {
  return emulator_->GetPPUContext();
}

CPUContext DebugPort::GetCPUContext() {
  return emulator_->GetCPUContext();
}

Byte DebugPort::CPUReadByte(Address address, bool* can_read) {
  if (address < 0x4000 || address > 0x401f) {
    if (can_read)
      *can_read = true;
    return emulator_->GetCPUMemory(address);
  }

  // Reading address between $4000-$401f has side effects, so we return a magic
  // byte 0xff.
  if (can_read)
    *can_read = false;
  return 0;
}

Byte DebugPort::PPUReadByte(Address address, bool* can_read) {
  if (can_read)
    *can_read = true;
  return emulator_->GetPPUMemory(address);
}

Byte DebugPort::OAMReadByte(Address address, bool* can_read) {
  DCHECK(0x00 <= address && address <= 0xff);
  if (0x00 <= address && address <= 0xff) {
    if (can_read)
      *can_read = true;
    return emulator_->GetOAMMemory(static_cast<Byte>(address));
  }

  if (can_read)
    *can_read = false;
  return 0;
}

void DebugPort::SetNametableRenderer(IODevices::RenderDevice* render_device) {
  nametable_render_device_ = render_device;
}

Colors DebugPort::GetPatternTableBGRA(PaletteName palette_name) {
  if (emulator_->GetRunningState() == Emulator::RunningState::kStopped)
    return Colors();
  
  // See https://www.nesdev.org/wiki/PPU_pattern_tables for tiles generation.
  // $0xx0-0xx7 are plane 0, $0xx8-0xxF are plane 1.
  Colors bgra(kPatternTableRows * kTwoPatternTablePixelsPerLine);

  // |indices| is only used when |palette_name| is not PaletteName::kIndexOnly.
  Byte indices[4];
  GetPaletteIndices(palette_name, indices);

  Palette* palette = emulator_->GetPPUContext().palette;
  DCHECK(palette);

  // Left
  Address base_row = 0, base_col = 0;
  for (Address i = 0; i < 0x1000; i += 0x10) {
    // Each tile is 16 bytes. Each tile is 8x8 pixels.
    for (Address row = 0; row < 8; ++row) {
      Byte bits[2] = {
          PPUReadByte(i + row),
          PPUReadByte(i + row + 8),
      };
      // |b| stands for each bit of the byte.
      for (Byte b = 0; b < 8; ++b) {
        Address vector_index = LEFT(base_row + row, base_col + b);
        bgra[vector_index] = BIT(bits[0], 7 - b) | (BIT(bits[1], 7 - b) << 1);
        if (palette_name != PaletteName::kIndexOnly) {
          bgra[vector_index] =
              palette->GetColorBGRA(indices[bgra[vector_index]]);
        }
      }
    }
    base_col += 8;
    if ((base_col + 128) % 256 == 0) {
      base_col = 0;
      base_row += 8;
    }
  }

  // Right
  base_row = 0, base_col = 0;
  for (int i = 0x1000; i < 0x2000; i += 0x10) {
    for (Address row = 0; row < 8; ++row) {
      Byte bits[2] = {
          PPUReadByte(i + row),
          PPUReadByte(i + row + 8),
      };
      // |b| stands for each bit of the byte.
      for (Byte b = 0; b < 8; ++b) {
        Address vector_index = RIGHT(base_row + row, base_col + b);
        bgra[vector_index] = BIT(bits[0], 7 - b) | (BIT(bits[1], 7 - b) << 1);
        if (palette_name != PaletteName::kIndexOnly) {
          bgra[vector_index] =
              palette->GetColorBGRA(indices[bgra[vector_index]]);
        }
      }
    }
    base_col += 8;
    if ((base_col + 128) % 256 == 0) {
      base_col = 0;
      base_row += 8;
    }
  }

  return bgra;
}

void DebugPort::GetPaletteIndices(PaletteName palette_name,
                                  Byte (&indices)[4]) {
  // See https://www.nesdev.org/wiki/PPU_palettes for more details.
  Address palette_base_address = 0;
  switch (palette_name) {
    case PaletteName::kBackgroundPalette0:
      palette_base_address = 0x3f01;
      break;
    case PaletteName::kBackgroundPalette1:
      palette_base_address = 0x3f05;
      break;
    case PaletteName::kBackgroundPalette2:
      palette_base_address = 0x3f09;
      break;
    case PaletteName::kBackgroundPalette3:
      palette_base_address = 0x3f0d;
      break;
    case PaletteName::kSpritePalette0:
      palette_base_address = 0x3f11;
      break;
    case PaletteName::kSpritePalette1:
      palette_base_address = 0x3f15;
      break;
    case PaletteName::kSpritePalette2:
      palette_base_address = 0x3f19;
      break;
    case PaletteName::kSpritePalette3:
      palette_base_address = 0x3f1d;
      break;
    case PaletteName::kIndexOnly:
      break;
    default:
      LOG(ERROR) << "Wrong palette name: " << static_cast<int>(palette_name);
      break;
  }

  indices[0] = PPUReadByte(0x3f00);
  indices[1] = PPUReadByte(palette_base_address);
  indices[2] = PPUReadByte(palette_base_address + 1);
  indices[3] = PPUReadByte(palette_base_address + 2);
}

Colors DebugPort::GetNametableBGRA() {
  if (emulator_->GetRunningState() == Emulator::RunningState::kStopped)
    return Colors();

  Colors bgra(kNametableWidth * kNametableHeight * 4);
  Address data_address = 0;
  Byte background_color = 0;
  for (int y = 0; y < 240 * 2; ++y) {
    for (int x = 0; x < 256 * 2; ++x) {
      Byte x_fine = x % 8;
      // Fetch tile (nametable byte).
      Address pixel_address = 0x2000 | (data_address & 0x0fff);
      Byte tile = PPUReadByte(pixel_address);
      // Gets tile address with fine Y scroll
      pixel_address = (tile << 4) + ((data_address >> 12) & 0x7);
      pixel_address +=
          (GetPPUContext().registers.PPUCTRL.B == 0 ? 0x0000 : 0x1000);

      Byte pattern = PPUReadByte(pixel_address);
      // Combines the tile and get background color index.
      background_color = (pattern >> (7 ^ x_fine)) & 1;
      background_color |= ((PPUReadByte(pixel_address + 8) >> (7 ^ x_fine)) & 1)
                          << 1;
      bool opaque = (background_color != 0);
      Address attribute_address = 0x23c0 | (data_address & 0x0c00) |
                                  ((data_address >> 4) & 0x38) |
                                  ((data_address >> 2) & 0x07);
      Byte attribute = PPUReadByte(attribute_address);
      int shift = ((data_address >> 4) & 4) | (data_address & 2);
      // Extract and set the upper two bits for the color
      background_color |= ((attribute >> shift) & 0x3) << 2;

      // Increment/wrap coarse X:
      // https://www.nesdev.org/wiki/PPU_scrolling#Wrapping_around
      if (x_fine == 7) {
        if ((data_address & 0x001f) == 31)  // if coarse X == 31
        {
          data_address &= ~0x001f;  // coarse X = 0
          data_address ^= 0x0400;   // switch horizontal nametable
        } else {
          data_address += 1;  // increment coarse X
        }
      }

      if (!opaque) {
        background_color = 0;
      }

      Palette* palette = emulator_->GetPPUContext().palette;
      DCHECK(palette);
      bgra[y * 512 + x] = (palette->GetColorBGRA(
          PPUReadByte(static_cast<Address>(background_color | 0x3f00))));
    }

    if ((data_address & 0x7000) != 0x7000) {
      data_address += 0x1000;
    } else {
      data_address &= ~0x7000;
      int new_y = (data_address & 0x03e0) >> 5;
      if (new_y == 29) {
        new_y = 0;
        data_address ^= 0x0800;
      } else if (new_y == 31) {
        new_y = 0;
      } else {
        new_y += 1;
      }
      data_address = (data_address & ~0x03e0) | (new_y << 5);
    }
  }

  return bgra;
}

Sprite DebugPort::GetSpriteInfo(Byte index) {
  Address address = static_cast<Address>(index) << 2;
  Byte oam[4] = {
      OAMReadByte(address),
      OAMReadByte(address + 1),
      OAMReadByte(address + 2),
      OAMReadByte(address + 3),
  };
  Sprite sprite;
  sprite.position.y = oam[0];
  sprite.position.x = oam[3];

  Colors pattern_table = GetPatternTableBGRA(PaletteName::kIndexOnly);
  const int kSpriteWidth = 8;
  int sprite_height = 8;

  PPURegisters r = emulator_->GetPPUContext().registers;
  sprite.is_8x8 = !r.PPUCTRL.H;
  if (sprite.is_8x8) {
    sprite.bgra.resize(kSpriteWidth * sprite_height);

    size_t pattern_table_pixel_offset =
        !emulator_->GetPPUContext().registers.PPUCTRL.S
            ? 0
            : (kTwoPatternTablePixelsPerLine / 2);
    Byte tile_index = oam[1];
    Byte tile_pos_x = tile_index % 16;
    Byte tile_pos_y = tile_index / 16;
    int pixel_pos_x =
        static_cast<int>(pattern_table_pixel_offset + (tile_pos_x * kTileSize));
    int pixel_pos_y = static_cast<int>(tile_pos_y * kTileSize);
    PaletteName palette_name = static_cast<PaletteName>(4 + (oam[2] & 0x3));
    CopyTileBGRA(pattern_table, &sprite.bgra, palette_name,
                 kTwoPatternTablePixelsPerLine, kSpriteWidth, pixel_pos_x,
                 pixel_pos_y, 0, 0);
  } else {
    sprite_height = 16;
    sprite.bgra.resize(kSpriteWidth * sprite_height);

    Byte tile_index = oam[1] & 0xfe;

    // Top 8x8
    {
      Byte tile_pos_x = tile_index % 16;
      Byte tile_pos_y = tile_index / 16;
      int pixel_pos_x = tile_pos_x * kTileSize;
      int pixel_pos_y = tile_pos_y * kTileSize;
      PaletteName palette_name = static_cast<PaletteName>(4 + (oam[2] & 0x3));
      CopyTileBGRA(pattern_table, &sprite.bgra, palette_name,
                   kTwoPatternTablePixelsPerLine, kSpriteWidth, pixel_pos_x,
                   pixel_pos_y, 0, 0);
    }
    // Bottom 8x8
    {
      Byte tile_pos_x = tile_index % 16;
      Byte tile_pos_y = tile_index / 16;
      int pixel_pos_x =
          (kTwoPatternTablePixelsPerLine / 2) + (tile_pos_x * kTileSize);
      int pixel_pos_y = tile_pos_y * kTileSize;
      PaletteName palette_name = static_cast<PaletteName>(4 + (oam[2] & 0x3));
      CopyTileBGRA(pattern_table, &sprite.bgra, palette_name,
                   kTwoPatternTablePixelsPerLine, kSpriteWidth, pixel_pos_x,
                   pixel_pos_y, 0, 8);
    }
  }

  bool flip_x = (oam[2] >> 6) & 1;
  bool flip_y = (oam[2] >> 7) & 1;
  if (flip_x) {
    for (int i = 0; i < sprite_height; ++i) {
      for (int j = 0; j < kSpriteWidth / 2; ++j) {
        std::swap(sprite.bgra[i * kSpriteWidth + j],
                  sprite.bgra[i * kSpriteWidth + (kSpriteWidth - j - 1)]);
      }
    }
  }

  if (flip_y) {
    for (int i = 0; i < kSpriteWidth; ++i) {
      for (int j = 0; j < sprite_height / 2; ++j) {
        std::swap(sprite.bgra[j * kSpriteWidth + i],
                  sprite.bgra[(sprite_height - j - 1) + i]);
      }
    }
  }

  return sprite;
}

Colors DebugPort::GetCurrentFrame() {
  return emulator_->GetCurrentFrame();
}

void DebugPort::SetAudioChannelMasks(int audio_channels) {
  emulator_->SetAudioChannelMasks(audio_channels);
}

int DebugPort::GetAudioChannelMasks() {
  return emulator_->GetAudioChannelMasks();
}

// A nametable is a 1024 byte area of memory used by the PPU to lay out
// backgrounds. Each byte in the nametable controls one 8x8 pixel character
// cell, and each nametable has 30 rows of 32 tiles each, for 960 ($3C0) bytes;
// the rest is used by each nametable's attribute table. With each tile being
// 8x8 pixels, this makes a total of 256x240 pixels in one map, the same size as
// one full screen.
Attributes DebugPort::GetNametableAttributes(Address nametable_start) {
  DCHECK(nametable_start == 0x2000 || nametable_start == 0x2400 ||
         nametable_start == 0x2800 || nametable_start == 0x2c00);
  const Address kAttributeTableStart =
      nametable_start + 0x400 - kAttributeTableSize;
  const Address kNametableEnd = nametable_start + 0x400;

  Attributes attributes(kAttributeTableSize);
  for (Address i = kAttributeTableStart; i < kNametableEnd; ++i) {
    attributes[i - kAttributeTableStart].value = PPUReadByte(i);
  }
  return attributes;
}

void DebugPort::CopyTileBGRA(const Colors& source_indices,
                             Colors* destination,
                             PaletteName palette_name,
                             int source_width,
                             int dest_width,
                             int source_x,
                             int source_y,
                             int dest_x,
                             int dest_y) {
  Palette* palette = emulator_->GetPPUContext().palette;
  DCHECK(palette);
  Byte palette_indices[4];
  GetPaletteIndices(palette_name, palette_indices);

  for (size_t x = 0; x < kTileSize; ++x) {
    for (size_t y = 0; y < kTileSize; ++y) {
      Color index =
          source_indices[source_x + x + source_width * (source_y + y)];
      DCHECK(index >= 0 && index <= 3);
      (*destination)[dest_x + x + dest_width * (dest_y + y)] =
          palette->GetColorBGRA(palette_indices[index]);
    }
  }
}

}  // namespace nes
}  // namespace kiwi
