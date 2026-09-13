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

#ifndef NES_COMPONENTS_MESEN_HD_PACK_HD_PACK_TYPES_H_
#define NES_COMPONENTS_MESEN_HD_PACK_HD_PACK_TYPES_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace kiwi {
namespace nes {
namespace mesen_hd_pack {

inline constexpr uint32_t kCurrentHiresVersion = 109;
inline constexpr size_t kChrTileByteCount = 16;
inline constexpr size_t kPaletteColorCount = 4;

enum class TileDataSource {
  kChrRom,
  kChrRam,
};

struct TileKey {
  TileDataSource source = TileDataSource::kChrRom;
  uint32_t chr_rom_index = 0;
  std::array<uint8_t, kChrTileByteCount> chr_data = {};
  std::array<uint8_t, kPaletteColorCount> palette = {};
};

enum class ConditionType {
  kHorizontalMirror,
  kVerticalMirror,
  kBackgroundPriority,
  kSpritePalette,
  kTileAtPosition,
  kTileNearby,
  kSpriteAtPosition,
  kSpriteNearby,
  kMemoryCheck,
  kPpuMemoryCheck,
  kMemoryCheckConstant,
  kPpuMemoryCheckConstant,
  kFrameRange,
  kPositionCheckX,
  kPositionCheckY,
  kOriginPositionCheckX,
  kOriginPositionCheckY,
};

enum class ComparisonOperator {
  kEqual,
  kNotEqual,
  kGreaterThan,
  kLessThan,
  kGreaterThanOrEqual,
  kLessThanOrEqual,
};

struct TileConditionData {
  int32_t x = 0;
  int32_t y = 0;
  TileKey tile;
  bool ignore_palette = false;
};

struct MemoryConditionData {
  uint32_t left_operand = 0;
  ComparisonOperator comparison = ComparisonOperator::kEqual;
  uint32_t right_operand = 0;
  uint8_t mask = 0xff;
};

struct FrameRangeConditionData {
  uint32_t divisor = 0;
  uint32_t compare_value = 0;
};

struct PositionConditionData {
  ComparisonOperator comparison = ComparisonOperator::kEqual;
  uint32_t position = 0;
};

struct SpritePaletteConditionData {
  uint8_t palette_index = 0;
};

using ConditionData = std::variant<std::monostate,
                                   TileConditionData,
                                   MemoryConditionData,
                                   FrameRangeConditionData,
                                   PositionConditionData,
                                   SpritePaletteConditionData>;

struct Condition {
  std::string name;
  ConditionType type = ConditionType::kHorizontalMirror;
  ConditionData data;
};

struct ConditionRef {
  size_t condition_index = 0;
  bool negated = false;
};

struct TileRule {
  size_t image_index = 0;
  TileKey tile;
  uint32_t image_x = 0;
  uint32_t image_y = 0;
  float brightness = 1.0f;
  bool default_for_tile = false;
  std::optional<uint32_t> chr_ram_bank_id;
  std::optional<uint32_t> chr_ram_tile_index;
  std::vector<ConditionRef> conditions;
};

enum class BackgroundBlendMode {
  kAlpha,
  kAdd,
  kSubtract,
};

struct BackgroundRule {
  std::string image_file;
  float brightness = 1.0f;
  float horizontal_scroll_ratio = 0.0f;
  float vertical_scroll_ratio = 0.0f;
  uint8_t priority = 10;
  uint32_t image_left = 0;
  uint32_t image_top = 0;
  BackgroundBlendMode blend_mode = BackgroundBlendMode::kAlpha;
  std::vector<ConditionRef> conditions;
};

struct AdditionalSpriteRule {
  TileKey original_tile;
  int32_t offset_x = 0;
  int32_t offset_y = 0;
  TileKey additional_tile;
  bool ignore_palette = false;
};

struct FallbackRule {
  uint32_t tile_index = 0;
  uint32_t fallback_tile_index = 0;
};

struct PatchRule {
  std::string file;
  std::string rom_sha1;
};

struct AudioRule {
  uint8_t album = 0;
  uint8_t track = 0;
  std::string file;
  std::optional<uint32_t> loop_position;
};

struct Overscan {
  int32_t top = 0;
  int32_t right = 0;
  int32_t bottom = 0;
  int32_t left = 0;
};

enum class HdPackOption : uint32_t {
  kDisableSpriteLimit = 1U << 0,
  kAlternateRegisterRange = 1U << 1,
  kDisableCache = 1U << 2,
  kDisableOriginalTiles = 1U << 3,
  kAutomaticFallbackTiles = 1U << 4,
};

struct HdPackData {
  uint32_t version = 0;
  uint32_t scale = 1;
  uint32_t options = 0;
  std::optional<Overscan> overscan;
  std::vector<std::string> supported_rom_sha1s;
  std::vector<std::string> image_files;
  std::vector<Condition> conditions;
  std::vector<TileRule> tiles;
  std::vector<BackgroundRule> backgrounds;
  std::vector<AdditionalSpriteRule> additional_sprites;
  std::vector<FallbackRule> fallback_tiles;
  std::vector<PatchRule> patches;
  std::vector<AudioRule> bgm_tracks;
  std::vector<AudioRule> sfx_tracks;
};

struct ParseError {
  size_t line = 0;
  std::string message;
};

}  // namespace mesen_hd_pack
}  // namespace nes
}  // namespace kiwi

#endif  // NES_COMPONENTS_MESEN_HD_PACK_HD_PACK_TYPES_H_
