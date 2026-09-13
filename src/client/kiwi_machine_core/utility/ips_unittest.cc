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

#include "utility/ips.h"

#include <cstdint>
#include <vector>

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace {

TEST(IpsTest, AppliesRawAndRleRecordsAndTruncatesInPlace) {
  const std::vector<uint8_t> patch = {
      'P',  'A',  'T',  'C',  'H',
      0x00, 0x00, 0x01, 0x00, 0x03, 0xaa, 0xbb, 0xcc,
      0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x03, 0xdd,
      'E',  'O',  'F',  0x00, 0x00, 0x07,
  };
  std::vector<uint8_t> data = {'a', 'b', 'c'};

  ASSERT_TRUE(ApplyIpsPatch(patch, &data));
  const std::vector<uint8_t> expected = {
      'a', 0xaa, 0xbb, 0xcc, 0x00, 0x00, 0xdd,
  };
  EXPECT_EQ(data, expected);
}

TEST(IpsTest, LeavesDataUnchangedWhenPatchIsMalformed) {
  const std::vector<uint8_t> patch = {
      'P', 'A', 'T', 'C', 'H', 0x00, 0x00, 0x01, 0x00,
  };
  const std::vector<uint8_t> original = {'a', 'b', 'c'};
  std::vector<uint8_t> data = original;

  EXPECT_FALSE(ApplyIpsPatch(patch, &data));
  EXPECT_EQ(data, original);
}

}  // namespace
