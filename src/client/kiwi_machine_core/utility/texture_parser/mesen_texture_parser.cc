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

#include "utility/texture_parser/mesen_texture_parser.h"

#include <SDL.h>
#include <SDL_image.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>

#include "nes/components/mesen_hd_pack/hires_parser.h"
#include "nes/components/mesen_hd_pack/rom_hash.h"
#include "nes/ppu_observer.h"
#include "utility/ips.h"
#include "utility/texture_parser/texture_resource_provider.h"
#include "utility/texture_renderer.h"

namespace {

constexpr int kScreenWidth = 256;
constexpr int kScreenHeight = 240;

void AddHashByte(uint8_t value, uint64_t* hash) {
  *hash ^= value;
  *hash *= 1099511628211ULL;
}

bool CompareSpriteOamIndex(const kiwi::nes::PPUTextureTileCommand* lhs,
                           const kiwi::nes::PPUTextureTileCommand* rhs) {
  return lhs->oam_index > rhs->oam_index;
}

uint32_t AlphaBlendChannel(uint32_t destination_channel,
                           uint32_t source_channel,
                           uint32_t alpha,
                           uint32_t inverse_alpha) {
  return (source_channel * alpha + destination_channel * inverse_alpha + 0x7f) /
         0xff;
}

bool NormalizeArchiveRoot(std::string* archive_root) {
  std::replace(archive_root->begin(), archive_root->end(), '\\', '/');
  while (!archive_root->empty() && archive_root->back() == '/') {
    archive_root->pop_back();
  }
  if (!archive_root->empty() && archive_root->front() == '/') {
    return false;
  }
  if (archive_root->empty()) {
    return true;
  }

  size_t component_start = 0;
  while (component_start <= archive_root->size()) {
    const size_t component_end = archive_root->find('/', component_start);
    const std::string_view component =
        component_end == std::string::npos
            ? std::string_view(*archive_root).substr(component_start)
            : std::string_view(*archive_root)
                  .substr(component_start, component_end - component_start);
    if (component.empty() || component == "." || component == "..") {
      return false;
    }
    if (component_end == std::string::npos) {
      break;
    }
    component_start = component_end + 1;
  }
  return true;
}

std::string JoinArchivePath(std::string_view root, std::string_view file) {
  if (root.empty()) {
    return std::string(file);
  }
  std::string result(root);
  result.push_back('/');
  result.append(file);
  return result;
}

using HdPackData = kiwi::nes::mesen_hd_pack::HdPackData;
using TileDataSource = kiwi::nes::mesen_hd_pack::TileDataSource;
using TileRule = kiwi::nes::mesen_hd_pack::TileRule;

struct MesenTextureImage {
  uint32_t width = 0;
  uint32_t height = 0;
  kiwi::nes::Colors pixels;
};

std::optional<MesenTextureImage> DecodeMesenTextureImage(
    const kiwi::nes::Bytes& image_data) {
  if (image_data.empty() ||
      image_data.size() >
          static_cast<size_t>(std::numeric_limits<int>::max())) {
    return std::nullopt;
  }

  SDL_RWops* source = SDL_RWFromConstMem(image_data.data(),
                                         static_cast<int>(image_data.size()));
  if (!source) {
    return std::nullopt;
  }
  SDL_Surface* decoded = IMG_Load_RW(source, true);
  if (!decoded) {
    return std::nullopt;
  }

  SDL_Surface* converted =
      SDL_ConvertSurfaceFormat(decoded, SDL_PIXELFORMAT_ARGB8888, 0);
  SDL_FreeSurface(decoded);
  if (!converted || converted->w <= 0 || converted->h <= 0) {
    if (converted) {
      SDL_FreeSurface(converted);
    }
    return std::nullopt;
  }

  MesenTextureImage image;
  image.width = static_cast<uint32_t>(converted->w);
  image.height = static_cast<uint32_t>(converted->h);
  image.pixels.resize(static_cast<size_t>(image.width) * image.height);
  if (SDL_MUSTLOCK(converted) && SDL_LockSurface(converted) != 0) {
    SDL_FreeSurface(converted);
    return std::nullopt;
  }
  for (uint32_t y = 0; y < image.height; ++y) {
    const void* source_row =
        static_cast<const uint8_t*>(converted->pixels) + y * converted->pitch;
    std::memcpy(image.pixels.data() + static_cast<size_t>(y) * image.width,
                source_row,
                static_cast<size_t>(image.width) * sizeof(uint32_t));
  }
  if (SDL_MUSTLOCK(converted)) {
    SDL_UnlockSurface(converted);
  }
  SDL_FreeSurface(converted);
  return image;
}

struct MesenTileLookupKey {
  kiwi::nes::PPUTextureTile::Source source =
      kiwi::nes::PPUTextureTile::Source::kChrRom;
  uint32_t tile_index = 0;
  std::array<kiwi::nes::Byte, 16> chr_data = {};
  std::array<kiwi::nes::Byte, 4> palette = {};

  bool operator==(const MesenTileLookupKey& other) const {
    if (source != other.source || palette != other.palette) {
      return false;
    }
    return source == kiwi::nes::PPUTextureTile::Source::kChrRam
               ? chr_data == other.chr_data
               : tile_index == other.tile_index;
  }
};

struct MesenTileLookupKeyHash {
  size_t operator()(const MesenTileLookupKey& key) const {
    uint64_t hash = 1469598103934665603ULL;
    AddHashByte(static_cast<uint8_t>(key.source), &hash);
    if (key.source == kiwi::nes::PPUTextureTile::Source::kChrRam) {
      for (uint8_t value : key.chr_data) {
        AddHashByte(value, &hash);
      }
    } else {
      for (size_t shift = 0; shift < sizeof(key.tile_index); ++shift) {
        AddHashByte(static_cast<uint8_t>(key.tile_index >> (shift * 8)), &hash);
      }
    }
    for (uint8_t value : key.palette) {
      AddHashByte(value, &hash);
    }
    return static_cast<size_t>(hash);
  }
};

MesenTileLookupKey MakeMesenTileLookupKey(
    const kiwi::nes::mesen_hd_pack::TileKey& tile) {
  MesenTileLookupKey key;
  key.source = tile.source == TileDataSource::kChrRam
                   ? kiwi::nes::PPUTextureTile::Source::kChrRam
                   : kiwi::nes::PPUTextureTile::Source::kChrRom;
  key.tile_index = tile.chr_rom_index;
  key.chr_data = tile.chr_data;
  key.palette = tile.palette;
  return key;
}

MesenTileLookupKey MakeMesenTileLookupKey(
    const kiwi::nes::PPUTextureTile& tile) {
  MesenTileLookupKey key;
  key.source = tile.source;
  key.tile_index = tile.tile_index;
  key.chr_data = tile.chr_data;
  key.palette = tile.palette;
  return key;
}

bool TileMatches(const kiwi::nes::PPUTextureTile& actual,
                 const kiwi::nes::mesen_hd_pack::TileKey& expected,
                 bool ignore_palette) {
  MesenTileLookupKey actual_key = MakeMesenTileLookupKey(actual);
  MesenTileLookupKey expected_key = MakeMesenTileLookupKey(expected);
  if (ignore_palette) {
    expected_key.palette = actual_key.palette;
  }
  return actual_key == expected_key;
}

bool HasMatchingTile(
    const kiwi::nes::PPUTextureTileCommand* command,
    const kiwi::nes::mesen_hd_pack::TileConditionData& condition) {
  return command &&
         TileMatches(command->tile, condition.tile, condition.ignore_palette);
}

bool HasMatchingTile(
    std::span<const kiwi::nes::PPUTextureTileCommand> commands,
    int x,
    int y,
    const kiwi::nes::mesen_hd_pack::TileConditionData& condition) {
  if (x < 0 || x >= kScreenWidth || y < 0 || y >= kScreenHeight) {
    return false;
  }
  return std::any_of(
      commands.begin(), commands.end(),
      [x, y, &condition](const kiwi::nes::PPUTextureTileCommand& command) {
        const int tile_x = x - command.x;
        const int tile_y = y - command.y;
        return tile_x >= 0 && tile_x < 8 && tile_y >= 0 && tile_y < 8 &&
               (command.visible_mask &
                (uint64_t{1} << (static_cast<size_t>(tile_y) * 8 + tile_x))) &&
               TileMatches(command.tile, condition.tile,
                           condition.ignore_palette);
      });
}

bool CompareMemoryValues(
    uint8_t left,
    uint8_t right,
    kiwi::nes::mesen_hd_pack::ComparisonOperator comparison) {
  using kiwi::nes::mesen_hd_pack::ComparisonOperator;
  switch (comparison) {
    case ComparisonOperator::kEqual:
      return left == right;
    case ComparisonOperator::kNotEqual:
      return left != right;
    case ComparisonOperator::kGreaterThan:
      return left > right;
    case ComparisonOperator::kLessThan:
      return left < right;
    case ComparisonOperator::kGreaterThanOrEqual:
      return left >= right;
    case ComparisonOperator::kLessThanOrEqual:
      return left <= right;
  }
  return false;
}

std::optional<uint8_t> ReadPPUPalette(
    const kiwi::nes::PPUFrameData& frame,
    uint32_t address) {
  constexpr uint32_t kPaletteBaseAddress = 0x3f00;
  constexpr uint32_t kPPUAddressSpaceSize = 0x4000;
  constexpr size_t kPaletteSize = 0x20;
  if (address < kPaletteBaseAddress || address >= kPPUAddressSpaceSize ||
      frame.texture_ppu_palette.size() != kPaletteSize) {
    return std::nullopt;
  }
  return frame.texture_ppu_palette[address & (kPaletteSize - 1)];
}

class MesenTextureRenderer final : public TextureRenderer {
 private:
  static constexpr size_t kNoRule = std::numeric_limits<size_t>::max();

  struct PreparedRule {
    kiwi::nes::Colors pixels;
    bool fully_opaque = true;
    bool fully_transparent = true;
  };

  struct PreparedBackground {
    kiwi::nes::Colors pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    bool fully_opaque = false;
  };

  struct MatchCache {
    MesenTileLookupKey key;
    size_t rule_index = kNoRule;
    bool valid = false;
  };

 public:
  MesenTextureRenderer(HdPackData pack_data,
                       std::vector<MesenTextureImage> images,
                       std::vector<MesenTextureImage> background_images)
      : pack_data_(std::move(pack_data)), output_scale_(pack_data_.scale) {
    const uint32_t tile_size = 8 * texture_scale();
    prepared_rules_.reserve(pack_data_.tiles.size());
    for (size_t index = 0; index < pack_data_.tiles.size(); ++index) {
      const TileRule& rule = pack_data_.tiles[index];
      uses_chr_ram_ |= rule.tile.source == TileDataSource::kChrRam;
      const MesenTextureImage& image = images[rule.image_index];
      PreparedRule prepared_rule;
      prepared_rule.pixels.reserve(static_cast<size_t>(tile_size) * tile_size);
      for (uint32_t y = 0; y < tile_size; ++y) {
        const size_t source_offset =
            static_cast<size_t>(rule.image_y + y) * image.width + rule.image_x;
        for (uint32_t x = 0; x < tile_size; ++x) {
          kiwi::nes::Color color = image.pixels[source_offset + x];
          if (rule.brightness != 1.0f) {
            color = AdjustBrightness(color, rule.brightness);
          }
          prepared_rule.fully_opaque &= (color >> 24) == 0xff;
          prepared_rule.fully_transparent &= (color >> 24) == 0;
          prepared_rule.pixels.push_back(color);
        }
      }
      prepared_rules_.push_back(std::move(prepared_rule));

      MesenTileLookupKey key = MakeMesenTileLookupKey(rule.tile);
      if (rule.default_for_tile) {
        key.palette.fill(0xff);
        default_rules_[key].push_back(index);
      } else {
        rules_[key].push_back(index);
      }
    }

    prepared_backgrounds_.reserve(pack_data_.backgrounds.size());
    for (MesenTextureImage& image : background_images) {
      PreparedBackground prepared_background;
      prepared_background.width = image.width;
      prepared_background.height = image.height;
      prepared_background.pixels = std::move(image.pixels);
      prepared_background.fully_opaque = std::all_of(
          prepared_background.pixels.begin(),
          prepared_background.pixels.end(),
          [](kiwi::nes::Color color) { return (color >> 24) == 0xff; });
      prepared_backgrounds_.push_back(std::move(prepared_background));
    }
  }

  ~MesenTextureRenderer() override = default;

  MesenTextureRenderer(const MesenTextureRenderer&) = delete;
  MesenTextureRenderer& operator=(const MesenTextureRenderer&) = delete;

  using TextureRenderer::RenderFrame;
  void SetMaximumOutputScale(uint32_t maximum_scale) override {
    output_scale_ =
        std::min(texture_scale(), std::max(uint32_t{1}, maximum_scale));
  }
  uint32_t GetScale() const override { return output_scale(); }
  bool IsUseChrRam() const override { return uses_chr_ram_; }

 private:
  uint32_t texture_scale() const { return pack_data_.scale; }
  uint32_t output_scale() const { return output_scale_; }

  uint32_t GetSourceSubpixel(uint32_t output_subpixel) const {
    return ((output_subpixel * 2 + 1) * texture_scale()) /
           (output_scale() * 2);
  }

  uint32_t GetSourceCoordinate(uint32_t output_coordinate) const {
    return output_coordinate / output_scale() * texture_scale() +
           GetSourceSubpixel(output_coordinate % output_scale());
  }

  uint32_t GetSourceTileCoordinate(uint32_t output_coordinate,
                                   bool mirrored) const {
    const uint32_t source_coordinate = GetSourceCoordinate(output_coordinate);
    return mirrored ? 8 * texture_scale() - source_coordinate - 1
                    : source_coordinate;
  }

  bool DrawTile(const kiwi::nes::PPUTextureTileCommand& command,
                const kiwi::nes::PPUFrameData& frame,
                uint64_t frame_number,
                kiwi::nes::Color* output,
                size_t output_stride,
                kiwi::nes::Byte* sprite_mask) const {
    const kiwi::nes::PPUTextureTile& tile = command.tile;
    MesenTileLookupKey key = MakeMesenTileLookupKey(tile);
    // Legacy packs store only the three visible palette colors. Mesen omitted
    // the universal background/sprite marker byte from runtime lookup as well.
    if (pack_data_.version < 100) {
      key.palette[0] = 0;
    }
    MatchCache& cache =
        tile.palette[0] == 0xff ? sprite_cache_ : background_cache_;
    if (cache.valid && cache.key == key) {
      if (cache.rule_index == kNoRule) {
        return false;
      }
      DrawRule(cache.rule_index, command, output, output_stride, sprite_mask);
      return true;
    }

    bool cacheable = true;
    const size_t rule_index =
        FindMatchingRule(key, command, frame, frame_number, &cacheable);
    if (cacheable) {
      cache.key = key;
      cache.rule_index = rule_index;
      cache.valid = true;
    }
    if (rule_index == kNoRule) {
      return false;
    }
    DrawRule(rule_index, command, output, output_stride, sprite_mask);
    return true;
  }

  bool render_original_tiles() const {
    using kiwi::nes::mesen_hd_pack::HdPackOption;
    return (pack_data_.options &
            static_cast<uint32_t>(HdPackOption::kDisableOriginalTiles)) == 0;
  }

 public:
  bool RenderFrame(const kiwi::nes::PPUFrameData& frame,
                   const TextureRenderTarget& target) const override {
    const size_t output_width = frame.width * output_scale();
    const size_t output_height = frame.height * output_scale();
    if (!target.pixels ||
        frame.type != kiwi::nes::PPUFrameData::Type::kTextureMetadata ||
        frame.width != kScreenWidth || frame.height != kScreenHeight ||
        target.width != static_cast<int>(output_width) ||
        target.height != static_cast<int>(output_height) ||
        target.stride < output_width ||
        frame.texture_backdrop_pixels.size() !=
            static_cast<size_t>(frame.width) * frame.height ||
        (!frame.texture_scroll_offsets.empty() &&
         frame.texture_scroll_offsets.size() !=
             static_cast<size_t>(frame.height))) {
      return false;
    }

    BlitBackdrop(frame, target);
    const uint64_t frame_number = frame_number_++;
    BuildSpatialIndex(frame);
    // OAM order remains authoritative across both background-priority passes.
    sprite_oam_mask_.assign(
        target.stride * static_cast<size_t>(target.height),
        std::numeric_limits<kiwi::nes::Byte>::max());
    DrawBackgrounds(frame, frame_number, 0, 10, target);

    std::vector<const kiwi::nes::PPUTextureTileCommand*> sprites;
    sprites.reserve(frame.texture_sprite_tiles.size());
    for (const kiwi::nes::PPUTextureTileCommand& sprite :
         frame.texture_sprite_tiles) {
      sprites.push_back(&sprite);
    }
    std::stable_sort(sprites.begin(), sprites.end(), CompareSpriteOamIndex);

    for (const kiwi::nes::PPUTextureTileCommand* sprite : sprites) {
      if (sprite->tile.background_priority) {
        DrawCommand(*sprite, frame, frame_number, true, target);
      }
    }
    DrawBackgrounds(frame, frame_number, 10, 20, target);

    for (const kiwi::nes::PPUTextureTileCommand& background :
         frame.texture_background_tiles) {
      DrawCommand(background, frame, frame_number, render_original_tiles(),
                  target);
    }
    DrawBackgrounds(frame, frame_number, 20, 30, target);

    for (const kiwi::nes::PPUTextureTileCommand* sprite : sprites) {
      if (!sprite->tile.background_priority) {
        DrawCommand(*sprite, frame, frame_number, true, target);
      }
    }
    DrawBackgrounds(frame, frame_number, 30, 40, target);

    return true;
  }

 private:
  void BuildSpatialIndex(const kiwi::nes::PPUFrameData& frame) const {
    background_at_pixel_.fill(nullptr);
    for (const kiwi::nes::PPUTextureTileCommand& command :
         frame.texture_background_tiles) {
      IndexBackgroundCommand(command);
    }
  }

  void IndexBackgroundCommand(
      const kiwi::nes::PPUTextureTileCommand& command) const {
    for (int tile_y = 0; tile_y < 8; ++tile_y) {
      const int y = command.y + tile_y;
      if (y < 0 || y >= kScreenHeight) {
        continue;
      }
      for (int tile_x = 0; tile_x < 8; ++tile_x) {
        const int x = command.x + tile_x;
        const size_t pixel_index = static_cast<size_t>(tile_y) * 8 + tile_x;
        if (x < 0 || x >= kScreenWidth ||
            (command.visible_mask & (uint64_t{1} << pixel_index)) == 0) {
          continue;
        }
        const size_t screen_index = static_cast<size_t>(y) * kScreenWidth + x;
        background_at_pixel_[screen_index] = &command;
      }
    }
  }

  void DrawBackgrounds(const kiwi::nes::PPUFrameData& frame,
                       uint64_t frame_number,
                       uint8_t priority_begin,
                       uint8_t priority_end,
                       const TextureRenderTarget& target) const {
    for (uint8_t priority = priority_begin; priority < priority_end;
         ++priority) {
      for (size_t index = 0; index < pack_data_.backgrounds.size(); ++index) {
        const auto& background = pack_data_.backgrounds[index];
        if (background.priority != priority ||
            !BackgroundConditionsMatch(background, frame, frame_number)) {
          continue;
        }
        DrawBackground(background, prepared_backgrounds_[index], frame, target);
        break;
      }
    }
  }

  void DrawBackground(
      const kiwi::nes::mesen_hd_pack::BackgroundRule& background,
      const PreparedBackground& prepared,
      const kiwi::nes::PPUFrameData& frame,
      const TextureRenderTarget& target) const {
    const int64_t source_scale = texture_scale();
    const bool direct_copy = output_scale() == texture_scale();
    for (int y = 0; y < target.height; ++y) {
      int32_t scroll_x = 0;
      int32_t scroll_y = 0;
      if (!frame.texture_scroll_offsets.empty()) {
        const auto& scroll =
            frame.texture_scroll_offsets[static_cast<size_t>(y) /
                                         output_scale()];
        scroll_x =
            static_cast<int32_t>(scroll.x * background.horizontal_scroll_ratio);
        scroll_y =
            static_cast<int32_t>(scroll.y * background.vertical_scroll_ratio);
      }

      const int64_t source_y =
          (static_cast<int64_t>(background.image_top) + scroll_y) *
              source_scale +
          (direct_copy ? y : GetSourceCoordinate(y));
      if (source_y < 0 || source_y >= prepared.height) {
        continue;
      }
      const int64_t source_origin_x =
          (static_cast<int64_t>(background.image_left) + scroll_x) *
          source_scale;

      if (direct_copy) {
        const int64_t first_x = std::max<int64_t>(0, -source_origin_x);
        const int64_t end_x =
            std::min<int64_t>(target.width, prepared.width - source_origin_x);
        if (first_x >= end_x) {
          continue;
        }

        kiwi::nes::Color* destination =
            target.pixels + static_cast<size_t>(y) * target.stride + first_x;
        const kiwi::nes::Color* source =
            prepared.pixels.data() +
            static_cast<size_t>(source_y) * prepared.width + source_origin_x +
            first_x;
        const size_t pixel_count = static_cast<size_t>(end_x - first_x);
        if (prepared.fully_opaque && background.brightness == 1.f &&
            background.blend_mode ==
                kiwi::nes::mesen_hd_pack::BackgroundBlendMode::kAlpha) {
          std::copy_n(source, pixel_count, destination);
          continue;
        }
        for (size_t x = 0; x < pixel_count; ++x) {
          kiwi::nes::Color color = source[x];
          if (background.brightness != 1.f) {
            color = AdjustBrightness(color, background.brightness);
          }
          destination[x] = AlphaBlend(destination[x], color);
        }
        continue;
      }

      kiwi::nes::Color* destination =
          target.pixels + static_cast<size_t>(y) * target.stride;
      for (int x = 0; x < target.width; ++x) {
        const int64_t source_x =
            source_origin_x + GetSourceCoordinate(x);
        if (source_x < 0 || source_x >= prepared.width) {
          continue;
        }
        kiwi::nes::Color color =
            prepared.pixels[static_cast<size_t>(source_y) * prepared.width +
                            source_x];
        if (background.brightness != 1.f) {
          color = AdjustBrightness(color, background.brightness);
        }
        destination[x] =
            prepared.fully_opaque
                ? color
                : AlphaBlend(destination[x], color);
      }
    }
  }

  bool BackgroundConditionsMatch(
      const kiwi::nes::mesen_hd_pack::BackgroundRule& background,
      const kiwi::nes::PPUFrameData& frame,
      uint64_t frame_number) const {
    TileRule condition_holder;
    condition_holder.conditions = background.conditions;
    kiwi::nes::PPUTextureTileCommand command;
    return ConditionsMatch(condition_holder, command, frame, frame_number);
  }

  void BlitBackdrop(const kiwi::nes::PPUFrameData& frame,
                    const TextureRenderTarget& target) const {
    const size_t output_width = frame.width * output_scale();
    for (int y = 0; y < frame.height; ++y) {
      kiwi::nes::Color* first_row =
          target.pixels +
          static_cast<size_t>(y * output_scale()) * target.stride;
      const kiwi::nes::Color* source_row =
          frame.texture_backdrop_pixels.data() +
          static_cast<size_t>(y) * frame.width;
      int run_start = 0;
      for (int x = 1; x <= frame.width; ++x) {
        if (x == frame.width || source_row[x] != source_row[run_start]) {
          std::fill_n(first_row + run_start * output_scale(),
                      static_cast<size_t>(x - run_start) * output_scale(),
                      source_row[run_start]);
          run_start = x;
        }
      }
      for (uint32_t row = 1; row < output_scale(); ++row) {
        std::copy_n(first_row, output_width, first_row + row * target.stride);
      }
    }
  }

  void DrawCommand(const kiwi::nes::PPUTextureTileCommand& command,
                   const kiwi::nes::PPUFrameData& frame,
                   uint64_t frame_number,
                   bool render_original,
                   const TextureRenderTarget& target) const {
    kiwi::nes::Byte* sprite_mask =
        command.tile.palette[0] == 0xff ? sprite_oam_mask_.data() : nullptr;
    if (!DrawTile(command, frame, frame_number, target.pixels, target.stride,
                  sprite_mask) &&
        render_original) {
      BlitOriginalTile(command, target.pixels, target.stride, sprite_mask);
    }
  }

  void BlitOriginalTile(const kiwi::nes::PPUTextureTileCommand& command,
                        kiwi::nes::Color* output,
                        size_t output_stride,
                        kiwi::nes::Byte* sprite_mask) const {
    const uint64_t pixels = command.visible_mask & command.opaque_mask;
    for (uint32_t tile_y = 0; tile_y < 8; ++tile_y) {
      for (uint32_t tile_x = 0; tile_x < 8; ++tile_x) {
        const size_t pixel_index = tile_y * 8 + tile_x;
        if ((pixels & (uint64_t{1} << pixel_index)) == 0) {
          continue;
        }

        const int screen_x = command.x + static_cast<int>(tile_x);
        const int screen_y = command.y + static_cast<int>(tile_y);
        if (screen_x < 0 || screen_x >= kScreenWidth || screen_y < 0 ||
            screen_y >= kScreenHeight) {
          continue;
        }
        kiwi::nes::Color* destination =
            output +
            static_cast<size_t>(screen_y * output_scale()) * output_stride +
            screen_x * output_scale();
        kiwi::nes::Byte* mask =
            sprite_mask
                ? sprite_mask +
                      static_cast<size_t>(screen_y * output_scale()) *
                          output_stride +
                      screen_x * output_scale()
                : nullptr;
        for (uint32_t row = 0; row < output_scale(); ++row) {
          if (!mask) {
            std::fill_n(destination, output_scale(),
                        command.native_colors[pixel_index]);
          } else {
            for (uint32_t x = 0; x < output_scale(); ++x) {
              if (command.oam_index <= mask[x]) {
                destination[x] = command.native_colors[pixel_index];
                mask[x] = command.oam_index;
              }
            }
            mask += output_stride;
          }
          destination += output_stride;
        }
      }
    }
  }

  size_t FindMatchingRule(MesenTileLookupKey key,
                          const kiwi::nes::PPUTextureTileCommand& command,
                          const kiwi::nes::PPUFrameData& frame,
                          uint64_t frame_number,
                          bool* cacheable) const {
    const std::vector<size_t>* candidates = nullptr;
    const auto exact = rules_.find(key);
    if (exact != rules_.end()) {
      candidates = &exact->second;
    } else {
      key.palette.fill(0xff);
      const auto default_rule = default_rules_.find(key);
      if (default_rule != default_rules_.end()) {
        candidates = &default_rule->second;
      }
    }
    if (!candidates) {
      return kNoRule;
    }

    for (size_t rule_index : *candidates) {
      const TileRule& rule = pack_data_.tiles[rule_index];
      if (!rule.conditions.empty()) {
        *cacheable = false;
      }
      if (ConditionsMatch(rule, command, frame, frame_number)) {
        return rule_index;
      }
    }
    return kNoRule;
  }

  bool ConditionsMatch(const TileRule& rule,
                       const kiwi::nes::PPUTextureTileCommand& command,
                       const kiwi::nes::PPUFrameData& frame,
                       uint64_t frame_number) const {
    using namespace kiwi::nes::mesen_hd_pack;
    const kiwi::nes::PPUTextureTile& tile = command.tile;
    for (const ConditionRef& condition_ref : rule.conditions) {
      if (condition_ref.condition_index >= pack_data_.conditions.size()) {
        return false;
      }

      const Condition& condition =
          pack_data_.conditions[condition_ref.condition_index];
      bool result = false;
      bool supported = true;
      switch (condition.type) {
        case ConditionType::kHorizontalMirror:
          result = tile.horizontal_mirroring;
          break;
        case ConditionType::kVerticalMirror:
          result = tile.vertical_mirroring;
          break;
        case ConditionType::kBackgroundPriority:
          result = tile.background_priority;
          break;
        case ConditionType::kSpritePalette: {
          const auto* sprite_palette =
              std::get_if<SpritePaletteConditionData>(&condition.data);
          result = sprite_palette &&
                   sprite_palette->palette_index == tile.sprite_palette;
          break;
        }
        case ConditionType::kTileAtPosition:
        case ConditionType::kTileNearby:
        case ConditionType::kSpriteAtPosition:
        case ConditionType::kSpriteNearby: {
          const auto* tile_condition =
              std::get_if<TileConditionData>(&condition.data);
          if (!tile_condition) {
            break;
          }
          const bool nearby =
              condition.type == ConditionType::kTileNearby ||
              condition.type == ConditionType::kSpriteNearby;
          const bool sprite =
              condition.type == ConditionType::kSpriteAtPosition ||
              condition.type == ConditionType::kSpriteNearby;
          int x = tile_condition->x;
          int y = tile_condition->y;
          if (nearby) {
            const int x_sign =
                sprite && tile.horizontal_mirroring ? -1 : 1;
            const int y_sign = sprite && tile.vertical_mirroring ? -1 : 1;
            x = command.x + tile_condition->x * x_sign;
            y = command.y + tile_condition->y * y_sign;
          }
          if (sprite) {
            result = HasMatchingTile(frame.texture_sprite_tiles, x, y,
                                     *tile_condition);
          } else if (x >= 0 && x < kScreenWidth && y >= 0 &&
                     y < kScreenHeight) {
            result = HasMatchingTile(
                background_at_pixel_[static_cast<size_t>(y) * kScreenWidth +
                                     x],
                *tile_condition);
          }
          break;
        }
        case ConditionType::kFrameRange: {
          const auto* frame_range =
              std::get_if<FrameRangeConditionData>(&condition.data);
          result = frame_range && frame_range->divisor != 0 &&
                   frame_number % frame_range->divisor >=
                       frame_range->compare_value;
          break;
        }
        case ConditionType::kPpuMemoryCheck:
        case ConditionType::kPpuMemoryCheckConstant: {
          const auto* memory =
              std::get_if<MemoryConditionData>(&condition.data);
          if (!memory) {
            break;
          }
          const std::optional<uint8_t> left =
              ReadPPUPalette(frame, memory->left_operand);
          const bool compares_constant =
              condition.type == ConditionType::kPpuMemoryCheckConstant;
          const std::optional<uint8_t> right =
              compares_constant
                  ? std::optional<uint8_t>(
                        static_cast<uint8_t>(memory->right_operand))
                  : ReadPPUPalette(frame, memory->right_operand);
          result = left && right &&
                   CompareMemoryValues(
                       static_cast<uint8_t>(*left & memory->mask),
                       compares_constant
                           ? *right
                           : static_cast<uint8_t>(*right & memory->mask),
                       memory->comparison);
          break;
        }
        default:
          supported = false;
          break;
      }
      if (!supported || (condition_ref.negated ? result : !result)) {
        return false;
      }
    }
    return true;
  }

  static uint8_t AdjustChannel(uint8_t channel, float brightness) {
    const float adjusted =
        std::clamp(static_cast<float>(channel) * brightness, 0.0f, 255.0f);
    return static_cast<uint8_t>(std::lround(adjusted));
  }

  static kiwi::nes::Color AdjustBrightness(kiwi::nes::Color color,
                                           float brightness) {
    const uint32_t alpha = color & 0xff000000;
    const uint8_t red = AdjustChannel((color >> 16) & 0xff, brightness);
    const uint8_t green = AdjustChannel((color >> 8) & 0xff, brightness);
    const uint8_t blue = AdjustChannel(color & 0xff, brightness);
    return alpha | (static_cast<uint32_t>(red) << 16) |
           (static_cast<uint32_t>(green) << 8) | blue;
  }

  static kiwi::nes::Color AlphaBlend(kiwi::nes::Color destination,
                                     kiwi::nes::Color source) {
    const uint32_t alpha = source >> 24;
    if (alpha == 0) {
      return destination;
    }
    if (alpha == 0xff) {
      return source;
    }

    const uint32_t inverse_alpha = 0xff - alpha;
    const uint32_t red =
        AlphaBlendChannel((destination >> 16) & 0xff, (source >> 16) & 0xff,
                          alpha, inverse_alpha);
    const uint32_t green = AlphaBlendChannel(
        (destination >> 8) & 0xff, (source >> 8) & 0xff, alpha, inverse_alpha);
    const uint32_t blue = AlphaBlendChannel(destination & 0xff, source & 0xff,
                                            alpha, inverse_alpha);
    return 0xff000000 | (red << 16) | (green << 8) | blue;
  }

  void DrawRule(size_t rule_index,
                const kiwi::nes::PPUTextureTileCommand& command,
                kiwi::nes::Color* output,
                size_t output_stride,
                kiwi::nes::Byte* sprite_mask) const {
    const PreparedRule& rule = prepared_rules_[rule_index];
    if (rule.fully_transparent) {
      return;
    }

    const kiwi::nes::PPUTextureTile& tile = command.tile;
    const uint32_t source_tile_size = 8 * texture_scale();
    const uint32_t output_tile_size = 8 * output_scale();
    if (!sprite_mask &&
        command.visible_mask == std::numeric_limits<uint64_t>::max() &&
        command.x >= 0 && command.x + 8 <= kScreenWidth && command.y >= 0 &&
        command.y + 8 <= kScreenHeight) {
      kiwi::nes::Color* destination =
          output +
          static_cast<size_t>(command.y * output_scale()) * output_stride +
          command.x * output_scale();
      for (uint32_t row = 0; row < output_tile_size; ++row) {
        const uint32_t source_y =
            GetSourceTileCoordinate(row, tile.vertical_mirroring);
        const kiwi::nes::Color* source_row =
            rule.pixels.data() +
            static_cast<size_t>(source_y) * source_tile_size;
        if (rule.fully_opaque && !tile.horizontal_mirroring &&
            output_scale() == texture_scale()) {
          std::copy_n(source_row, output_tile_size, destination);
        } else {
          for (uint32_t x = 0; x < output_tile_size; ++x) {
            const uint32_t source_x =
                GetSourceTileCoordinate(x, tile.horizontal_mirroring);
            destination[x] =
                AlphaBlend(destination[x], source_row[source_x]);
          }
        }
        destination += output_stride;
      }
      return;
    }

    for (uint32_t tile_y = 0; tile_y < 8; ++tile_y) {
      for (uint32_t tile_x = 0; tile_x < 8; ++tile_x) {
        const size_t pixel_index = tile_y * 8 + tile_x;
        if ((command.visible_mask & (uint64_t{1} << pixel_index)) == 0) {
          continue;
        }

        const int screen_x = command.x + static_cast<int>(tile_x);
        const int screen_y = command.y + static_cast<int>(tile_y);
        if (screen_x < 0 || screen_x >= kScreenWidth || screen_y < 0 ||
            screen_y >= kScreenHeight) {
          continue;
        }
        kiwi::nes::Color* destination =
            output +
            static_cast<size_t>(screen_y * output_scale()) * output_stride +
            screen_x * output_scale();
        kiwi::nes::Byte* mask =
            sprite_mask
                ? sprite_mask +
                      static_cast<size_t>(screen_y * output_scale()) *
                          output_stride +
                      screen_x * output_scale()
                : nullptr;
        for (uint32_t y = 0; y < output_scale(); ++y) {
          const uint32_t source_y = GetSourceTileCoordinate(
              tile_y * output_scale() + y, tile.vertical_mirroring);
          const kiwi::nes::Color* source_row =
              rule.pixels.data() +
              static_cast<size_t>(source_y) * source_tile_size;
          if (mask) {
            for (uint32_t x = 0; x < output_scale(); ++x) {
              const uint32_t source_x = GetSourceTileCoordinate(
                  tile_x * output_scale() + x, tile.horizontal_mirroring);
              const kiwi::nes::Color color = source_row[source_x];
              const uint32_t alpha = color >> 24;
              if (alpha == 0 || command.oam_index > mask[x]) {
                continue;
              }
              destination[x] = AlphaBlend(destination[x], color);
              if (alpha == 0xff) {
                mask[x] = command.oam_index;
              }
            }
            mask += output_stride;
          } else if (rule.fully_opaque && !tile.horizontal_mirroring &&
                     output_scale() == texture_scale()) {
            const uint32_t source_x = tile_x * texture_scale();
            std::copy_n(source_row + source_x, output_scale(), destination);
          } else {
            for (uint32_t x = 0; x < output_scale(); ++x) {
              const uint32_t source_x = GetSourceTileCoordinate(
                  tile_x * output_scale() + x, tile.horizontal_mirroring);
              destination[x] =
                  AlphaBlend(destination[x], source_row[source_x]);
            }
          }
          destination += output_stride;
        }
      }
    }
  }

  HdPackData pack_data_;
  std::vector<PreparedRule> prepared_rules_;
  std::vector<PreparedBackground> prepared_backgrounds_;
  std::unordered_map<MesenTileLookupKey,
                     std::vector<size_t>,
                     MesenTileLookupKeyHash>
      rules_;
  std::unordered_map<MesenTileLookupKey,
                     std::vector<size_t>,
                     MesenTileLookupKeyHash>
      default_rules_;
  mutable MatchCache background_cache_;
  mutable MatchCache sprite_cache_;
  mutable uint64_t frame_number_ = 0;
  mutable std::vector<kiwi::nes::Byte> sprite_oam_mask_;
  mutable std::array<const kiwi::nes::PPUTextureTileCommand*,
                     kScreenWidth * kScreenHeight>
      background_at_pixel_{};
  uint32_t output_scale_ = 1;
  bool uses_chr_ram_ = false;
};

}  // namespace

MesenTextureParser::MesenTextureParser(
    std::string archive_root,
    std::string_view hires_contents,
    const std::unordered_set<std::string>& archive_entries)
    : archive_root_(std::move(archive_root)) {
  if (!NormalizeArchiveRoot(&archive_root_)) {
    parse_errors_.push_back({0, "Invalid Mesen HD Pack archive root"});
    return;
  }

  kiwi::nes::mesen_hd_pack::HiresParser parser;
  definition_valid_ = parser.Parse(hires_contents, &pack_data_, &parse_errors_);
  if (definition_valid_) {
    resources_available_ = HasRequiredResources(archive_entries);
  }
}

bool MesenTextureParser::Verify(std::span<const uint8_t> rom_data) const {
  if (!definition_valid_ || !resources_available_) {
    return false;
  }

  const std::string sha1 = kiwi::nes::mesen_hd_pack::CalculateSha1Hex(rom_data);
  return std::find(pack_data_.supported_rom_sha1s.begin(),
                   pack_data_.supported_rom_sha1s.end(),
                   sha1) != pack_data_.supported_rom_sha1s.end();
}

bool MesenTextureParser::HasRomPatch(
    std::span<const uint8_t> rom_data) const {
  if (!Verify(rom_data)) {
    return false;
  }

  const std::string sha1 = kiwi::nes::mesen_hd_pack::CalculateSha1Hex(rom_data);
  return std::any_of(
      pack_data_.patches.begin(), pack_data_.patches.end(),
      [&sha1](const kiwi::nes::mesen_hd_pack::PatchRule& patch) {
        return patch.rom_sha1 == sha1;
      });
}

bool MesenTextureParser::ApplyRomPatch(
    std::vector<uint8_t>* rom_data,
    TextureResourceProvider& resources) const {
  if (!rom_data || !Verify(*rom_data)) {
    return false;
  }

  const std::string sha1 =
      kiwi::nes::mesen_hd_pack::CalculateSha1Hex(*rom_data);
  const auto patch = std::find_if(
      pack_data_.patches.begin(), pack_data_.patches.end(),
      [&sha1](const kiwi::nes::mesen_hd_pack::PatchRule& candidate) {
        return candidate.rom_sha1 == sha1;
      });
  if (patch == pack_data_.patches.end()) {
    return false;
  }

  std::optional<kiwi::nes::Bytes> patch_data =
      resources.ReadFile(JoinArchivePath(archive_root_, patch->file));
  return patch_data && ApplyIpsPatch(*patch_data, rom_data);
}

std::unique_ptr<TextureRenderer> MesenTextureParser::CreateTextureRenderer(
    std::span<const uint8_t> rom_data,
    TextureResourceProvider& resources) const {
  if (!Verify(rom_data)) {
    return nullptr;
  }

  std::vector<MesenTextureImage> images;
  images.reserve(pack_data_.image_files.size());
  for (const std::string& image_file : pack_data_.image_files) {
    std::optional<kiwi::nes::Bytes> image_data =
        resources.ReadFile(JoinArchivePath(archive_root_, image_file));
    if (!image_data) {
      return nullptr;
    }
    std::optional<MesenTextureImage> image =
        DecodeMesenTextureImage(*image_data);
    if (!image) {
      return nullptr;
    }
    images.push_back(std::move(*image));
  }

  for (const MesenTextureImage& image : images) {
    const size_t pixel_count = static_cast<size_t>(image.width) * image.height;
    if (image.width == 0 || image.height == 0 ||
        image.pixels.size() != pixel_count) {
      return nullptr;
    }
  }

  std::vector<MesenTextureImage> background_images;
  background_images.reserve(pack_data_.backgrounds.size());
  for (const auto& background : pack_data_.backgrounds) {
    std::optional<kiwi::nes::Bytes> image_data = resources.ReadFile(
        JoinArchivePath(archive_root_, background.image_file));
    if (!image_data) {
      return nullptr;
    }
    std::optional<MesenTextureImage> image =
        DecodeMesenTextureImage(*image_data);
    if (!image) {
      return nullptr;
    }
    background_images.push_back(std::move(*image));
  }

  const uint64_t tile_size = static_cast<uint64_t>(pack_data_.scale) * 8;
  for (const TileRule& rule : pack_data_.tiles) {
    if (rule.image_index >= images.size()) {
      return nullptr;
    }
    const MesenTextureImage& image = images[rule.image_index];
    if (static_cast<uint64_t>(rule.image_x) + tile_size > image.width ||
        static_cast<uint64_t>(rule.image_y) + tile_size > image.height) {
      return nullptr;
    }
  }

  return std::make_unique<MesenTextureRenderer>(
      pack_data_, std::move(images), std::move(background_images));
}

bool MesenTextureParser::HasRequiredResources(
    const std::unordered_set<std::string>& archive_entries) const {
  std::set<std::string> resources = {"hires.txt"};
  resources.insert(pack_data_.image_files.begin(),
                   pack_data_.image_files.end());
  for (const auto& background : pack_data_.backgrounds) {
    resources.insert(background.image_file);
  }
  for (const auto& patch : pack_data_.patches) {
    resources.insert(patch.file);
  }
  for (const auto& bgm : pack_data_.bgm_tracks) {
    resources.insert(bgm.file);
  }
  for (const auto& sfx : pack_data_.sfx_tracks) {
    resources.insert(sfx.file);
  }

  for (const std::string& resource : resources) {
    if (!archive_entries.contains(JoinArchivePath(archive_root_, resource))) {
      return false;
    }
  }
  return true;
}

MesenTextureParserCollection::MesenTextureParserCollection(
    std::vector<std::unique_ptr<MesenTextureParser>> parsers)
    : parsers_(std::move(parsers)) {}

bool MesenTextureParserCollection::Verify(
    std::span<const uint8_t> rom_data) const {
  for (const std::unique_ptr<MesenTextureParser>& parser : parsers_) {
    if (parser->Verify(rom_data)) {
      return true;
    }
  }
  return false;
}

bool MesenTextureParserCollection::HasRomPatch(
    std::span<const uint8_t> rom_data) const {
  for (const std::unique_ptr<MesenTextureParser>& parser : parsers_) {
    if (parser->Verify(rom_data)) {
      return parser->HasRomPatch(rom_data);
    }
  }
  return false;
}

bool MesenTextureParserCollection::ApplyRomPatch(
    std::vector<uint8_t>* rom_data,
    TextureResourceProvider& resources) const {
  if (!rom_data) {
    return false;
  }
  for (const std::unique_ptr<MesenTextureParser>& parser : parsers_) {
    if (parser->Verify(*rom_data)) {
      return parser->ApplyRomPatch(rom_data, resources);
    }
  }
  return false;
}

std::unique_ptr<TextureRenderer>
MesenTextureParserCollection::CreateTextureRenderer(
    std::span<const uint8_t> rom_data,
    TextureResourceProvider& resources) const {
  for (const std::unique_ptr<MesenTextureParser>& parser : parsers_) {
    std::unique_ptr<TextureRenderer> renderer =
        parser->CreateTextureRenderer(rom_data, resources);
    if (renderer) {
      return renderer;
    }
  }
  return nullptr;
}
