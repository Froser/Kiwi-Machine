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
#include <string>
#include <unordered_map>
#include <utility>

#include "nes/components/mesen_hd_pack/hires_parser.h"
#include "nes/components/mesen_hd_pack/rom_hash.h"
#include "nes/ppu_observer.h"
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
  if (archive_root->empty() || archive_root->front() == '/') {
    return false;
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

class MesenTextureRenderer final : public TextureRenderer {
 private:
  static constexpr size_t kNoRule = std::numeric_limits<size_t>::max();

  struct PreparedRule {
    kiwi::nes::Colors pixels;
    bool fully_opaque = true;
    bool fully_transparent = true;
  };

  struct MatchCache {
    MesenTileLookupKey key;
    size_t rule_index = kNoRule;
    bool valid = false;
  };

 public:
  MesenTextureRenderer(HdPackData pack_data,
                       std::vector<MesenTextureImage> images)
      : pack_data_(std::move(pack_data)) {
    const uint32_t tile_size = 8 * scale();
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
  }

  ~MesenTextureRenderer() override = default;

  MesenTextureRenderer(const MesenTextureRenderer&) = delete;
  MesenTextureRenderer& operator=(const MesenTextureRenderer&) = delete;

  using TextureRenderer::RenderFrame;
  uint32_t GetScale() const override { return scale(); }
  bool IsUseChrRam() const override { return uses_chr_ram_; }

 private:
  uint32_t scale() const { return pack_data_.scale; }

  bool DrawTile(const kiwi::nes::PPUTextureTileCommand& command,
                kiwi::nes::Color* output,
                size_t output_stride) const {
    const kiwi::nes::PPUTextureTile& tile = command.tile;
    MesenTileLookupKey key = MakeMesenTileLookupKey(tile);
    MatchCache& cache =
        tile.palette[0] == 0xff ? sprite_cache_ : background_cache_;
    if (cache.valid && cache.key == key) {
      if (cache.rule_index == kNoRule) {
        return false;
      }
      DrawRule(cache.rule_index, command, output, output_stride);
      return true;
    }

    bool cacheable = true;
    const size_t rule_index = FindMatchingRule(key, tile, &cacheable);
    if (cacheable) {
      cache.key = key;
      cache.rule_index = rule_index;
      cache.valid = true;
    }
    if (rule_index == kNoRule) {
      return false;
    }
    DrawRule(rule_index, command, output, output_stride);
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
    const size_t output_width = frame.width * scale();
    const size_t output_height = frame.height * scale();
    if (!target.pixels ||
        frame.type != kiwi::nes::PPUFrameData::Type::kTextureMetadata ||
        frame.width != kScreenWidth || frame.height != kScreenHeight ||
        target.width != static_cast<int>(output_width) ||
        target.height != static_cast<int>(output_height) ||
        target.stride < output_width ||
        frame.texture_backdrop_pixels.size() !=
            static_cast<size_t>(frame.width) * frame.height) {
      return false;
    }

    BlitBackdrop(frame, target);

    std::vector<const kiwi::nes::PPUTextureTileCommand*> sprites;
    sprites.reserve(frame.texture_sprite_tiles.size());
    for (const kiwi::nes::PPUTextureTileCommand& sprite :
         frame.texture_sprite_tiles) {
      sprites.push_back(&sprite);
    }
    std::stable_sort(sprites.begin(), sprites.end(), CompareSpriteOamIndex);

    for (const kiwi::nes::PPUTextureTileCommand* sprite : sprites) {
      if (sprite->tile.background_priority) {
        DrawCommand(*sprite, true, target);
      }
    }

    for (const kiwi::nes::PPUTextureTileCommand& background :
         frame.texture_background_tiles) {
      DrawCommand(background, render_original_tiles(), target);
    }

    for (const kiwi::nes::PPUTextureTileCommand* sprite : sprites) {
      if (!sprite->tile.background_priority) {
        DrawCommand(*sprite, true, target);
      }
    }

    return true;
  }

 private:
  void BlitBackdrop(const kiwi::nes::PPUFrameData& frame,
                    const TextureRenderTarget& target) const {
    const size_t output_width = frame.width * scale();
    for (int y = 0; y < frame.height; ++y) {
      kiwi::nes::Color* first_row =
          target.pixels + static_cast<size_t>(y * scale()) * target.stride;
      const kiwi::nes::Color* source_row =
          frame.texture_backdrop_pixels.data() +
          static_cast<size_t>(y) * frame.width;
      int run_start = 0;
      for (int x = 1; x <= frame.width; ++x) {
        if (x == frame.width || source_row[x] != source_row[run_start]) {
          std::fill_n(first_row + run_start * scale(),
                      static_cast<size_t>(x - run_start) * scale(),
                      source_row[run_start]);
          run_start = x;
        }
      }
      for (uint32_t row = 1; row < scale(); ++row) {
        std::copy_n(first_row, output_width, first_row + row * target.stride);
      }
    }
  }

  void DrawCommand(const kiwi::nes::PPUTextureTileCommand& command,
                   bool render_original,
                   const TextureRenderTarget& target) const {
    if (!DrawTile(command, target.pixels, target.stride) && render_original) {
      BlitOriginalTile(command, target.pixels, target.stride);
    }
  }

  void BlitOriginalTile(const kiwi::nes::PPUTextureTileCommand& command,
                        kiwi::nes::Color* output,
                        size_t output_stride) const {
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
            output + static_cast<size_t>(screen_y * scale()) * output_stride +
            screen_x * scale();
        for (uint32_t row = 0; row < scale(); ++row) {
          std::fill_n(destination, scale(), command.native_colors[pixel_index]);
          destination += output_stride;
        }
      }
    }
  }

  size_t FindMatchingRule(MesenTileLookupKey key,
                          const kiwi::nes::PPUTextureTile& tile,
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
      if (ConditionsMatch(rule, tile)) {
        return rule_index;
      }
    }
    return kNoRule;
  }

  bool ConditionsMatch(const TileRule& rule,
                       const kiwi::nes::PPUTextureTile& tile) const {
    using namespace kiwi::nes::mesen_hd_pack;
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
                size_t output_stride) const {
    const PreparedRule& rule = prepared_rules_[rule_index];
    if (rule.fully_transparent) {
      return;
    }

    const kiwi::nes::PPUTextureTile& tile = command.tile;
    const uint32_t tile_size = 8 * scale();
    if (command.visible_mask == std::numeric_limits<uint64_t>::max() &&
        command.x >= 0 && command.x + 8 <= kScreenWidth && command.y >= 0 &&
        command.y + 8 <= kScreenHeight) {
      kiwi::nes::Color* destination =
          output + static_cast<size_t>(command.y * scale()) * output_stride +
          command.x * scale();
      for (uint32_t row = 0; row < tile_size; ++row) {
        const uint32_t source_y =
            tile.vertical_mirroring ? tile_size - row - 1 : row;
        const kiwi::nes::Color* source =
            rule.pixels.data() + static_cast<size_t>(source_y) * tile_size;
        if (rule.fully_opaque && !tile.horizontal_mirroring) {
          std::copy_n(source, tile_size, destination);
        } else {
          for (uint32_t x = 0; x < tile_size; ++x) {
            const uint32_t source_x =
                tile.horizontal_mirroring ? tile_size - x - 1 : x;
            destination[x] = AlphaBlend(destination[x], source[source_x]);
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
            output + static_cast<size_t>(screen_y * scale()) * output_stride +
            screen_x * scale();
        const uint32_t source_tile_x =
            tile.horizontal_mirroring ? 7 - tile_x : tile_x;
        const uint32_t source_tile_y =
            tile.vertical_mirroring ? 7 - tile_y : tile_y;
        for (uint32_t y = 0; y < scale(); ++y) {
          const uint32_t source_y =
              source_tile_y * scale() +
              (tile.vertical_mirroring ? scale() - y - 1 : y);
          const kiwi::nes::Color* source =
              rule.pixels.data() + static_cast<size_t>(source_y) * tile_size +
              source_tile_x * scale();
          if (rule.fully_opaque && !tile.horizontal_mirroring) {
            std::copy_n(source, scale(), destination);
          } else {
            for (uint32_t x = 0; x < scale(); ++x) {
              const uint32_t source_x =
                  tile.horizontal_mirroring ? scale() - x - 1 : x;
              destination[x] = AlphaBlend(destination[x], source[source_x]);
            }
          }
          destination += output_stride;
        }
      }
    }
  }

  HdPackData pack_data_;
  std::vector<PreparedRule> prepared_rules_;
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

  return std::make_unique<MesenTextureRenderer>(pack_data_, std::move(images));
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
