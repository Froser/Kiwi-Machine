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

#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
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

bool RenderBackgroundTile(TextureRenderer& renderer,
                          const kiwi::nes::PPUTextureTile& tile,
                          kiwi::nes::Color original_color,
                          kiwi::nes::Colors* output,
                          size_t output_stride = 0) {
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

}  // namespace
