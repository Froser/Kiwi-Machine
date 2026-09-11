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
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "nes/ppu_observer.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"
#include "utility/texture_parser/texture_resource_provider.h"
#include "utility/texture_renderer.h"

namespace {

class FakeTextureResourceProvider final : public TextureResourceProvider {
 public:
  FakeTextureResourceProvider() = default;
  ~FakeTextureResourceProvider() override = default;

  void AddFile(std::string path, kiwi::nes::Bytes data) {
    file_paths_.insert(path);
    files_.insert_or_assign(std::move(path), std::move(data));
  }

  const std::unordered_set<std::string>& GetFilePaths() const override {
    return file_paths_;
  }

  std::optional<kiwi::nes::Bytes> ReadFile(std::string_view path) override {
    const auto file = files_.find(std::string(path));
    if (file == files_.end()) {
      return std::nullopt;
    }
    return file->second;
  }

 private:
  std::unordered_set<std::string> file_paths_;
  std::unordered_map<std::string, kiwi::nes::Bytes> files_;
};

kiwi::nes::Bytes EncodePng(uint32_t width,
                           uint32_t height,
                           const kiwi::nes::Colors& pixels) {
  const size_t capacity = pixels.size() * sizeof(kiwi::nes::Color) + 4096;
  kiwi::nes::Bytes encoded(capacity);
  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
      const_cast<kiwi::nes::Color*>(pixels.data()), static_cast<int>(width),
      static_cast<int>(height), 32,
      static_cast<int>(width * sizeof(kiwi::nes::Color)),
      SDL_PIXELFORMAT_ARGB8888);
  if (!surface) {
    return {};
  }

  SDL_RWops* destination =
      SDL_RWFromMem(encoded.data(), static_cast<int>(encoded.size()));
  if (!destination) {
    SDL_FreeSurface(surface);
    return {};
  }
  const int save_result = IMG_SavePNG_RW(surface, destination, false);
  const Sint64 encoded_size = SDL_RWtell(destination);
  SDL_RWclose(destination);
  SDL_FreeSurface(surface);
  if (save_result != 0 || encoded_size < 0 ||
      static_cast<size_t>(encoded_size) > encoded.size()) {
    return {};
  }
  encoded.resize(static_cast<size_t>(encoded_size));
  return encoded;
}

bool RenderBackgroundTile(
    TextureRenderer& renderer,
    const kiwi::nes::PPUTextureTile& tile,
    kiwi::nes::Color original_color,
    kiwi::nes::Colors* output,
    size_t output_stride = 0,
    std::span<const kiwi::nes::Byte> ppu_palette = {},
    std::span<const kiwi::nes::PPUTextureScroll> scroll_offsets = {}) {
  constexpr int kWidth = 256;
  constexpr int kHeight = 240;
  kiwi::nes::Colors backdrop_pixels(kWidth * kHeight, original_color);
  std::vector<kiwi::nes::PPUTextureTileCommand> background_tiles(1);
  kiwi::nes::PPUTextureTileCommand& command = background_tiles.front();
  command.tile = tile;
  command.visible_mask = std::numeric_limits<uint64_t>::max();
  command.opaque_mask = command.visible_mask;
  command.native_colors.fill(original_color);

  kiwi::nes::PPUFrameData frame;
  frame.type = kiwi::nes::PPUFrameData::Type::kTextureMetadata;
  frame.width = kWidth;
  frame.height = kHeight;
  frame.texture_backdrop_pixels = backdrop_pixels;
  frame.texture_background_tiles = background_tiles;
  frame.texture_ppu_palette = ppu_palette;
  frame.texture_scroll_offsets = scroll_offsets;
  if (output_stride != 0) {
    constexpr kiwi::nes::Color kPaddingColor = 0xff123456;
    const int output_width = frame.width * renderer.GetScale();
    const int output_height = frame.height * renderer.GetScale();
    output->assign(output_stride * output_height, kPaddingColor);
    const TextureRenderTarget target = {
        output->data(),
        output_stride,
        output_width,
        output_height,
    };
    return renderer.RenderFrame(frame, target);
  }
  return renderer.RenderFrame(frame, output);
}

kiwi::nes::PPUTextureTileCommand MakeTileCommand(
    uint32_t tile_index,
    std::array<kiwi::nes::Byte, 4> palette,
    int x,
    int y) {
  kiwi::nes::PPUTextureTileCommand command;
  command.tile.tile_index = tile_index;
  command.tile.palette = palette;
  command.x = x;
  command.y = y;
  command.visible_mask = std::numeric_limits<uint64_t>::max();
  command.opaque_mask = command.visible_mask;
  command.native_colors.fill(0xff000000);
  return command;
}

TEST(MesenTextureParserTest, VerifiesRomWithoutLoadingTextureResources) {
  constexpr std::string_view kDefinition = R"(
<ver>109
<scale>4
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/tiles.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  EXPECT_TRUE(parser.definition_valid());
  EXPECT_EQ(parser.type(), TextureType::kMesen);
  EXPECT_TRUE(parser.Verify(kAbc));
}

TEST(MesenTextureParserTest, RejectsPackWithMissingResources) {
  constexpr std::string_view kDefinition = R"(
<ver>109
<scale>4
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>missing.png
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  EXPECT_TRUE(parser.definition_valid());
  EXPECT_FALSE(parser.Verify(kAbc));
}

TEST(MesenTextureParserTest, AppliesMatchingIpsRomPatch) {
  constexpr std::string_view kDefinition = R"(
<ver>109
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<patch>game.ips,A9993E364706816ABA3E25717850C26C9CD0D89D
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/game.ips",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};
  const kiwi::nes::Bytes patch = {
      'P',  'A',  'T',  'C',  'H',
      0x00, 0x00, 0x01, 0x00, 0x03, 0xaa, 0xbb, 0xcc,
      0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03, 0xdd,
      'E',  'O',  'F',  0x00, 0x00, 0x07,
  };
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/game.ips", patch);

  EXPECT_TRUE(parser.HasRomPatch(kAbc));
  kiwi::nes::Bytes patched_rom(kAbc.begin(), kAbc.end());
  ASSERT_TRUE(parser.ApplyRomPatch(&patched_rom, resources));
  const kiwi::nes::Bytes expected = {
      'a', 0xaa, 0xbb, 0xcc, 0x00, 0x00, 0xdd,
  };
  EXPECT_EQ(patched_rom, expected);
}

TEST(MesenTextureParserCollectionTest,
     VerifiesRomAndCreatesRendererFromMatchingNestedPack) {
  constexpr std::string_view kUnmatchedDefinition = R"(
<ver>109
<scale>4
<supportedRom>0000000000000000000000000000000000000000
<img>tiles.png
)";
  constexpr std::string_view kMatchedDefinition = R"(
<ver>109
<scale>4
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/game-a/hires.txt",
      "textures/mesen/game-a/tiles.png",
      "textures/mesen/game-b/hires.txt",
      "textures/mesen/game-b/tiles.png",
  };
  std::vector<std::unique_ptr<MesenTextureParser>> parsers;
  parsers.push_back(std::make_unique<MesenTextureParser>(
      "textures/mesen/game-a", kUnmatchedDefinition, archive_entries));
  parsers.push_back(std::make_unique<MesenTextureParser>(
      "textures/mesen/game-b", kMatchedDefinition, archive_entries));
  MesenTextureParserCollection collection(std::move(parsers));
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  EXPECT_EQ(collection.size(), 2u);
  EXPECT_EQ(collection.type(), TextureType::kMesen);
  EXPECT_TRUE(collection.Verify(kAbc));

  constexpr uint32_t kImageWidth = 32;
  constexpr uint32_t kImageHeight = 32;
  kiwi::nes::Colors pixels(kImageWidth * kImageHeight, 0xffabcdef);
  kiwi::nes::Bytes image_data = EncodePng(kImageWidth, kImageHeight, pixels);
  ASSERT_FALSE(image_data.empty());
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/game-b/tiles.png", std::move(image_data));
  std::unique_ptr<TextureRenderer> renderer =
      collection.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);
  EXPECT_EQ(renderer->GetScale(), 4u);
}

TEST(MesenTextureParserTest, CreatesRendererThatDrawsMatchingTilePixels) {
  constexpr std::string_view kDefinition = R"(
<ver>109
<scale>2
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
<tile>0,2A,0F112233,0,0,1,N
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/tiles.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  constexpr uint32_t kImageWidth = 16;
  constexpr uint32_t kImageHeight = 16;
  kiwi::nes::Colors pixels(kImageWidth * kImageHeight);
  pixels[8 * kImageWidth + 6] = 0x80ff0000;
  pixels[8 * kImageWidth + 7] = 0xff00ff00;
  pixels[8 * kImageWidth + 8] = 0xff445566;
  pixels[9 * kImageWidth + 6] = 0xff112233;
  pixels[9 * kImageWidth + 7] = 0xffffffff;
  kiwi::nes::Bytes image_data = EncodePng(kImageWidth, kImageHeight, pixels);
  ASSERT_FALSE(image_data.empty());
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/tiles.png", std::move(image_data));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);
  EXPECT_EQ(renderer->GetScale(), 2u);

  kiwi::nes::PPUTextureTile tile;
  tile.tile_index = 0x2a;
  tile.palette = {0x0f, 0x11, 0x22, 0x33};
  kiwi::nes::Colors output;

  EXPECT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff0000ff, &output));
  constexpr size_t kOutputWidth = 512;
  constexpr size_t kSampleOffset = 8 * kOutputWidth + 6;
  EXPECT_EQ(output[kSampleOffset], 0xff80007f);
  EXPECT_EQ(output[kSampleOffset + 1], 0xff00ff00);
  EXPECT_EQ(output[kSampleOffset + kOutputWidth], 0xff112233);
  EXPECT_EQ(output[kSampleOffset + kOutputWidth + 1], 0xffffffff);
  EXPECT_EQ(output[2], 0xff0000ff);
  EXPECT_EQ(output[8 * kOutputWidth + 8], 0xff445566);

  constexpr size_t kPaddedStride = kOutputWidth + 7;
  constexpr kiwi::nes::Color kPaddingColor = 0xff123456;
  EXPECT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff0000ff, &output,
                                   kPaddedStride));
  EXPECT_EQ(output[8 * kPaddedStride + 6], 0xff80007f);
  EXPECT_EQ(output[kOutputWidth], kPaddingColor);

  tile.palette[3] = 0x34;
  EXPECT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff0000ff, &output));
  EXPECT_EQ(output[0], 0xff0000ff);
}

TEST(MesenTextureParserTest, MatchesLegacyTileWithoutUniversalPaletteColor) {
  constexpr std::string_view kDefinition = R"(
<ver>2
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
<tile>0,0,22,39,24,0,0,1,N
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/tiles.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  FakeTextureResourceProvider resources;
  resources.AddFile(
      "textures/mesen/tiles.png",
      EncodePng(8, 8, kiwi::nes::Colors(8 * 8, 0xffabcdef)));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  kiwi::nes::PPUTextureTile tile;
  tile.tile_index = 0;
  tile.palette = {0x2a, 22, 39, 24};
  kiwi::nes::Colors output;
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[0], 0xffabcdefu);
}

TEST(MesenTextureParserTest,
     PreservesSpriteOamPriorityAcrossBackgroundPriorityPasses) {
  constexpr std::string_view kDefinition = R"(
<ver>109
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
<tile>0,1,FF112233,0,0,1,N
<tile>0,2,FF445566,8,0,1,N
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/tiles.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  constexpr uint32_t kImageWidth = 16;
  constexpr uint32_t kImageHeight = 8;
  kiwi::nes::Colors pixels(kImageWidth * kImageHeight, 0xffaa0000);
  for (uint32_t y = 0; y < kImageHeight; ++y) {
    std::fill_n(pixels.begin() + y * kImageWidth + 8, 8, 0xff00aa00);
  }
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/tiles.png",
                    EncodePng(kImageWidth, kImageHeight, pixels));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  kiwi::nes::PPUTextureTileCommand behind_sprite =
      MakeTileCommand(1, {0xff, 0x11, 0x22, 0x33}, 0, 0);
  behind_sprite.tile.background_priority = true;
  behind_sprite.oam_index = 0;
  kiwi::nes::PPUTextureTileCommand foreground_sprite =
      MakeTileCommand(2, {0xff, 0x44, 0x55, 0x66}, 0, 0);
  foreground_sprite.oam_index = 1;

  kiwi::nes::PPUFrameData frame;
  frame.type = kiwi::nes::PPUFrameData::Type::kTextureMetadata;
  frame.width = 256;
  frame.height = 240;
  kiwi::nes::Colors backdrop_pixels(256 * 240, 0xff000000);
  std::vector<kiwi::nes::PPUTextureTileCommand> sprite_tiles = {
      behind_sprite, foreground_sprite};
  frame.texture_backdrop_pixels = backdrop_pixels;
  frame.texture_sprite_tiles = sprite_tiles;
  kiwi::nes::Colors output;

  ASSERT_TRUE(renderer->RenderFrame(frame, &output));
  EXPECT_EQ(output[0], 0xffaa0000u);

  sprite_tiles[0].oam_index = 2;
  ASSERT_TRUE(renderer->RenderFrame(frame, &output));
  EXPECT_EQ(output[0], 0xff00aa00u);
}

TEST(MesenTextureParserTest, MatchesChrRamByAllSixteenTileBytes) {
  constexpr std::string_view kDefinition = R"(
<ver>109
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
<tile>0,000102030405060708090A0B0C0D0E0F,0F112233,0,0,1,N
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/tiles.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  constexpr uint32_t kImageWidth = 8;
  constexpr uint32_t kImageHeight = 8;
  kiwi::nes::Colors pixels(kImageWidth * kImageHeight, 0xffabcdef);
  kiwi::nes::Bytes image_data = EncodePng(kImageWidth, kImageHeight, pixels);
  ASSERT_FALSE(image_data.empty());
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/tiles.png", std::move(image_data));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  kiwi::nes::PPUTextureTile tile;
  tile.source = kiwi::nes::PPUTextureTile::Source::kChrRam;
  tile.tile_index = 1234;
  tile.palette = {0x0f, 0x11, 0x22, 0x33};
  for (size_t index = 0; index < tile.chr_data.size(); ++index) {
    tile.chr_data[index] = static_cast<uint8_t>(index);
  }
  kiwi::nes::Colors output;
  EXPECT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[0], 0xffabcdefu);

  tile.chr_data.back() = 0xff;
  EXPECT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[0], 0xff000000u);
}

TEST(MesenTextureParserTest, MatchesDonkeyKongSpatialConditions) {
  constexpr std::string_view kDefinition = R"(
<ver>101
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
<condition>near,tileNearby,8,0,43,0F112233
<condition>fixed,tileAtPosition,16,0,44,0F112233
<condition>sprite,spriteNearby,0,8,48,FF112233
[near&fixed&sprite]<tile>0,42,0F112233,0,0,1,N
<tile>0,42,0F112233,8,0,1,N
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/tiles.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  constexpr uint32_t kImageWidth = 16;
  constexpr uint32_t kImageHeight = 8;
  kiwi::nes::Colors pixels(kImageWidth * kImageHeight, 0xff00aa00);
  for (uint32_t y = 0; y < kImageHeight; ++y) {
    std::fill_n(pixels.begin() + y * kImageWidth + 8, 8, 0xffaa0000);
  }
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/tiles.png",
                    EncodePng(kImageWidth, kImageHeight, pixels));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  constexpr std::array<kiwi::nes::Byte, 4> kBackgroundPalette = {
      0x0f, 0x11, 0x22, 0x33};
  constexpr std::array<kiwi::nes::Byte, 4> kSpritePalette = {
      0xff, 0x11, 0x22, 0x33};
  std::vector<kiwi::nes::PPUTextureTileCommand> background_tiles = {
      MakeTileCommand(0x2a, kBackgroundPalette, 0, 0),
      MakeTileCommand(0x2b, kBackgroundPalette, 8, 0),
      MakeTileCommand(0x2c, kBackgroundPalette, 16, 0),
  };
  std::vector<kiwi::nes::PPUTextureTileCommand> sprite_tiles = {
      MakeTileCommand(0x30, kSpritePalette, 0, 8),
  };
  kiwi::nes::Colors backdrop_pixels(256 * 240, 0xff000000);
  kiwi::nes::PPUFrameData frame;
  frame.type = kiwi::nes::PPUFrameData::Type::kTextureMetadata;
  frame.native_pixels = &backdrop_pixels;
  frame.texture_backdrop_pixels = backdrop_pixels;
  frame.texture_background_tiles = background_tiles;
  frame.texture_sprite_tiles = sprite_tiles;
  kiwi::nes::Colors output;

  ASSERT_TRUE(renderer->RenderFrame(frame, &output));
  EXPECT_EQ(output[0], 0xff00aa00u);

  sprite_tiles.clear();
  frame.texture_sprite_tiles = sprite_tiles;
  ASSERT_TRUE(renderer->RenderFrame(frame, &output));
  EXPECT_EQ(output[0], 0xffaa0000u);
}

TEST(MesenTextureParserTest, MatchesFrameRangeCondition) {
  constexpr std::string_view kDefinition = R"(
<ver>101
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
<condition>odd,frameRange,2,1
[odd]<tile>0,42,0F112233,0,0,1,N
<tile>0,42,0F112233,8,0,1,N
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/tiles.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  constexpr uint32_t kImageWidth = 16;
  constexpr uint32_t kImageHeight = 8;
  kiwi::nes::Colors pixels(kImageWidth * kImageHeight, 0xff00aa00);
  for (uint32_t y = 0; y < kImageHeight; ++y) {
    std::fill_n(pixels.begin() + y * kImageWidth + 8, 8, 0xffaa0000);
  }
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/tiles.png",
                    EncodePng(kImageWidth, kImageHeight, pixels));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  kiwi::nes::PPUTextureTile tile;
  tile.tile_index = 0x2a;
  tile.palette = {0x0f, 0x11, 0x22, 0x33};
  kiwi::nes::Colors output;
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[0], 0xffaa0000u);
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[0], 0xff00aa00u);
}

TEST(MesenTextureParserTest, MatchesPPUPaletteMemoryConditions) {
  constexpr std::string_view kDefinition = R"(
<ver>103
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<img>tiles.png
<condition>fade,ppuMemoryCheckConstant,3F13,==,25,3F
<condition>same,ppuMemoryCheck,3F13,==,3F01
[fade&same]<tile>0,2A,0F112233,0,0,1,N
<tile>0,2A,0F112233,8,0,1,N
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/tiles.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  constexpr uint32_t kImageWidth = 16;
  constexpr uint32_t kImageHeight = 8;
  kiwi::nes::Colors pixels(kImageWidth * kImageHeight, 0xff00aa00);
  for (uint32_t y = 0; y < kImageHeight; ++y) {
    std::fill_n(pixels.begin() + y * kImageWidth + 8, 8, 0xffaa0000);
  }
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/tiles.png",
                    EncodePng(kImageWidth, kImageHeight, pixels));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  kiwi::nes::PPUTextureTile tile;
  tile.tile_index = 0x2a;
  tile.palette = {0x0f, 0x11, 0x22, 0x33};
  std::array<kiwi::nes::Byte, 0x20> ppu_palette = {};
  kiwi::nes::Colors output;

  ppu_palette[0x13] = 0x25;
  ppu_palette[0x01] = 0x25;
  ASSERT_TRUE(
      RenderBackgroundTile(*renderer, tile, 0xff000000, &output, 0,
                           ppu_palette));
  EXPECT_EQ(output[0], 0xff00aa00u);

  ppu_palette[0x01] = 0x15;
  ASSERT_TRUE(
      RenderBackgroundTile(*renderer, tile, 0xff000000, &output, 0,
                           ppu_palette));
  EXPECT_EQ(output[0], 0xffaa0000u);
}

TEST(MesenTextureParserTest, DrawsConditionalBackgroundLayer) {
  constexpr std::string_view kDefinition = R"(
<ver>101
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<condition>screen,tileAtPosition,0,0,42,0F112233
[screen]<background>background.png,1,0,0
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/background.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  FakeTextureResourceProvider resources;
  resources.AddFile(
      "textures/mesen/background.png",
      EncodePng(256, 240, kiwi::nes::Colors(256 * 240, 0xff00aa00)));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  kiwi::nes::PPUTextureTile tile;
  tile.tile_index = 42;
  tile.palette = {0x0f, 0x11, 0x22, 0x33};
  kiwi::nes::Colors output;
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[8], 0xff00aa00u);

  tile.tile_index = 43;
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[8], 0xff000000u);
}

TEST(MesenTextureParserTest, DrawsFirstMatchingBackgroundPerPriority) {
  constexpr std::string_view kDefinition = R"(
<ver>106
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<condition>late,frameRange,2,1
[late]<background>animated.png,1,0,0,0
<background>fallback.png,1,0,0,0
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/animated.png",
      "textures/mesen/fallback.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  FakeTextureResourceProvider resources;
  resources.AddFile(
      "textures/mesen/animated.png",
      EncodePng(256, 240, kiwi::nes::Colors(256 * 240, 0xffaa0000)));
  resources.AddFile(
      "textures/mesen/fallback.png",
      EncodePng(256, 240, kiwi::nes::Colors(256 * 240, 0xff00aa00)));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  kiwi::nes::PPUTextureTile tile;
  kiwi::nes::Colors output;
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[8], 0xff00aa00u);
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[8], 0xffaa0000u);
}

TEST(MesenTextureParserTest, AppliesBackgroundScrollRatios) {
  constexpr std::string_view kDefinition = R"(
<ver>106
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<background>background.png,1,1,1,10
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/background.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  constexpr uint32_t kImageWidth = 258;
  constexpr uint32_t kImageHeight = 243;
  kiwi::nes::Colors pixels(kImageWidth * kImageHeight, 0xff000000);
  pixels[3 * kImageWidth + 10] = 0xff00aa00;
  FakeTextureResourceProvider resources;
  resources.AddFile("textures/mesen/background.png",
                    EncodePng(kImageWidth, kImageHeight, pixels));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  std::array<kiwi::nes::PPUTextureScroll, 240> scroll_offsets;
  scroll_offsets.fill({2, 3});
  kiwi::nes::PPUTextureTile tile;
  kiwi::nes::Colors output;
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output, 0, {},
                                   scroll_offsets));
  EXPECT_EQ(output[8], 0xff00aa00u);
}

TEST(MesenTextureParserTest, DrawsNonBoundaryBackgroundPriorities) {
  constexpr std::array<uint8_t, 8> kPriorities = {
      1, 9, 11, 19, 21, 29, 31, 39,
  };
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/background.png",
  };
  const kiwi::nes::Bytes background_png =
      EncodePng(256, 240, kiwi::nes::Colors(256 * 240, 0xff00aa00));

  for (uint8_t priority : kPriorities) {
    SCOPED_TRACE(static_cast<int>(priority));
    const std::string definition =
        "<ver>106\n"
        "<scale>1\n"
        "<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D\n"
        "<background>background.png,1,0,0," +
        std::to_string(priority) + "\n";
    MesenTextureParser parser("textures/mesen", definition, archive_entries);
    FakeTextureResourceProvider resources;
    resources.AddFile("textures/mesen/background.png", background_png);
    std::unique_ptr<TextureRenderer> renderer =
        parser.CreateTextureRenderer(kAbc, resources);
    ASSERT_TRUE(renderer);

    kiwi::nes::PPUTextureTile tile;
    kiwi::nes::Colors output;
    ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
    EXPECT_EQ(output[8], 0xff00aa00u);
  }
}

TEST(MesenTextureParserTest, DrawsBackgroundsInPriorityOrder) {
  constexpr std::string_view kDefinition = R"(
<ver>106
<scale>1
<supportedRom>A9993E364706816ABA3E25717850C26C9CD0D89D
<background>high.png,1,0,0,9
<background>low.png,1,0,0,1
)";
  const std::unordered_set<std::string> archive_entries = {
      "textures/mesen/hires.txt",
      "textures/mesen/high.png",
      "textures/mesen/low.png",
  };
  MesenTextureParser parser("textures/mesen", kDefinition, archive_entries);
  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};

  FakeTextureResourceProvider resources;
  resources.AddFile(
      "textures/mesen/high.png",
      EncodePng(256, 240, kiwi::nes::Colors(256 * 240, 0xffaa0000)));
  resources.AddFile(
      "textures/mesen/low.png",
      EncodePng(256, 240, kiwi::nes::Colors(256 * 240, 0xff00aa00)));
  std::unique_ptr<TextureRenderer> renderer =
      parser.CreateTextureRenderer(kAbc, resources);
  ASSERT_TRUE(renderer);

  kiwi::nes::PPUTextureTile tile;
  kiwi::nes::Colors output;
  ASSERT_TRUE(RenderBackgroundTile(*renderer, tile, 0xff000000, &output));
  EXPECT_EQ(output[8], 0xffaa0000u);
}

}  // namespace
