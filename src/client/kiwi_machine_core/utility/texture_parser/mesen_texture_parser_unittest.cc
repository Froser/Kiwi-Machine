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

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace {

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

TEST(MesenTextureParserCollectionTest, VerifiesRomAgainstNestedPacks) {
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
}

}  // namespace
