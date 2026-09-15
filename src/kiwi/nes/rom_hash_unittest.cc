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

#include "nes/rom_hash.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace nes {
namespace {

TEST(RomHashTest, CalculatesKnownSha1Vectors) {
  EXPECT_EQ(CalculateSha1Hex(std::span<const uint8_t>()),
            "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709");

  constexpr std::array<uint8_t, 3> kAbc = {'a', 'b', 'c'};
  EXPECT_EQ(CalculateSha1Hex(kAbc), "A9993E364706816ABA3E25717850C26C9CD0D89D");
}

TEST(RomHashTest, NormalizesSha1Hex) {
  std::optional<std::string> normalized =
      NormalizeSha1Hex("a9993e364706816aba3e25717850c26c9cd0d89d");
  ASSERT_TRUE(normalized);
  EXPECT_EQ(*normalized, "A9993E364706816ABA3E25717850C26C9CD0D89D");
  EXPECT_FALSE(NormalizeSha1Hex("not-a-sha1"));
}

TEST(RomHashTest, CalculatesNES20RomContentHashAndBatterySaveKey) {
  constexpr size_t kHeaderSize = 16;
  constexpr size_t kContentSize = 16 * 1024 + 8 * 1024;
  std::vector<uint8_t> image(kHeaderSize + kContentSize);
  image[0] = 'N';
  image[1] = 'E';
  image[2] = 'S';
  image[3] = 0x1a;
  image[4] = 1;
  image[5] = 1;
  image[7] = 0x08;

  const std::string expected_sha1 =
      CalculateSha1Hex(std::span<const uint8_t>(image).subspan(kHeaderSize));
  EXPECT_EQ(CalculateINESRomSha1Hex(image), expected_sha1);
  EXPECT_FALSE(CalculateINESBatterySaveSha1Hex(image));

  image[6] = 0x02;
  EXPECT_EQ(CalculateINESBatterySaveSha1Hex(image), expected_sha1);
}

TEST(RomHashTest, RejectsInvalidINESRom) {
  EXPECT_FALSE(CalculateINESRomSha1Hex({}));
  EXPECT_FALSE(CalculateINESBatterySaveSha1Hex({}));

  std::vector<uint8_t> image(16, 0);
  EXPECT_FALSE(CalculateINESRomSha1Hex(image));

  image[0] = 'N';
  image[1] = 'E';
  image[2] = 'S';
  image[3] = 0x1a;
  image[4] = 1;
  EXPECT_FALSE(CalculateINESRomSha1Hex(image));

  image.resize(16 + 16 * 1024);
  image[6] = 0x04;
  EXPECT_FALSE(CalculateINESRomSha1Hex(image));
}

}  // namespace
}  // namespace nes
}  // namespace kiwi
