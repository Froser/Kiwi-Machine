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

#include "nes/components/mesen_hd_pack/hires_parser.h"

#include <string_view>
#include <vector>

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace nes {
namespace mesen_hd_pack {
namespace {

TEST(HiresParserTest, ParsesMesenVersion109Rules) {
  constexpr std::string_view kDefinition = R"(
<ver>109
<scale>4
<supportedRom>0123456789abcdef0123456789abcdef01234567
<overscan>8,7,6,5
<img>Tiles\atlas.png
<condition>title,tileAtPosition,0,0,10,0F100017,N
<condition>pulse,frameRange,60,30
[sppalette2&title&!pulse]<tile>0,2E,FF16360F,0,32,1.25,N
<tile>0,0E0E079C1E3EA7076101586121010000,0F100017,32,64,1,Y,3,7
[title]<background>Backgrounds\title.png,1,0.5,0,12,4,8,Add
<addition>2E,FF16360F,8,-4,31,FF16360F,N
<fallback>2E,30
<patch>patches\game.ips,0123456789abcdef0123456789abcdef01234567
<options>disableSpriteLimit,disableCache,automaticFallbackTiles
<bgm>1,2,audio\stage.ogg,44100
<sfx>1,3,audio\jump.ogg
)";

  HdPackData data;
  std::vector<ParseError> errors;
  HiresParser parser;
  ASSERT_TRUE(parser.Parse(kDefinition, &data, &errors));
  EXPECT_TRUE(errors.empty());

  EXPECT_EQ(data.version, 109U);
  EXPECT_EQ(data.scale, 4U);
  ASSERT_TRUE(data.overscan.has_value());
  EXPECT_EQ(data.overscan->left, 5);
  ASSERT_EQ(data.supported_rom_sha1s.size(), 1U);
  EXPECT_EQ(data.supported_rom_sha1s[0],
            "0123456789ABCDEF0123456789ABCDEF01234567");
  ASSERT_EQ(data.image_files.size(), 1U);
  EXPECT_EQ(data.image_files[0], "Tiles/atlas.png");

  ASSERT_EQ(data.conditions.size(), 9U);
  ASSERT_EQ(data.tiles.size(), 2U);
  EXPECT_EQ(data.tiles[0].tile.source, TileDataSource::kChrRom);
  EXPECT_EQ(data.tiles[0].tile.chr_rom_index, 0x2eU);
  EXPECT_EQ(data.tiles[0].conditions.size(), 3U);
  EXPECT_TRUE(data.tiles[0].conditions[2].negated);
  EXPECT_EQ(data.tiles[1].tile.source, TileDataSource::kChrRam);
  ASSERT_TRUE(data.tiles[1].chr_ram_bank_id.has_value());
  EXPECT_EQ(data.tiles[1].chr_ram_bank_id.value(), 3U);
  ASSERT_TRUE(data.tiles[1].chr_ram_tile_index.has_value());
  EXPECT_EQ(data.tiles[1].chr_ram_tile_index.value(), 7U);

  ASSERT_EQ(data.backgrounds.size(), 1U);
  EXPECT_EQ(data.backgrounds[0].image_file, "Backgrounds/title.png");
  EXPECT_EQ(data.backgrounds[0].priority, 12);
  EXPECT_EQ(data.backgrounds[0].blend_mode, BackgroundBlendMode::kAdd);
  EXPECT_EQ(data.additional_sprites.size(), 1U);
  EXPECT_EQ(data.additional_sprites[0].offset_y, -4);
  EXPECT_EQ(data.fallback_tiles.size(), 1U);
  EXPECT_EQ(data.patches.size(), 1U);
  EXPECT_EQ(data.bgm_tracks.size(), 1U);
  ASSERT_TRUE(data.bgm_tracks[0].loop_position.has_value());
  EXPECT_EQ(data.bgm_tracks[0].loop_position.value(), 44100U);
  EXPECT_EQ(data.sfx_tracks.size(), 1U);
}

TEST(HiresParserTest, ParsesDecimalTileIndexInVersion102) {
  constexpr std::string_view kDefinition = R"(
<ver>102
<scale>2
<img>tiles.png
<tile>0,46,FF16360F,0,0,1,N
)";

  HdPackData data;
  std::vector<ParseError> errors;
  HiresParser parser;
  ASSERT_TRUE(parser.Parse(kDefinition, &data, &errors));
  ASSERT_EQ(data.tiles.size(), 1U);
  EXPECT_EQ(data.tiles[0].tile.chr_rom_index, 46U);
}

TEST(HiresParserTest, ParsesBackgroundPriorityInLegacyVersion2) {
  constexpr std::string_view kDefinition = R"(
<ver>2
<scale>4
<background>background.png,1,0,0,Y
)";

  HdPackData data;
  std::vector<ParseError> errors;
  HiresParser parser;
  const bool parsed = parser.Parse(kDefinition, &data, &errors);
  ASSERT_TRUE(parsed);
  ASSERT_EQ(data.backgrounds.size(), 1U);
  EXPECT_EQ(data.backgrounds[0].priority, 0);
}

}  // namespace
}  // namespace mesen_hd_pack
}  // namespace nes
}  // namespace kiwi
