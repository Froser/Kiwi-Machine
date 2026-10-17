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

#include "models/gba/frame.h"

#include <type_traits>

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

static_assert(std::is_base_of_v<kiwi::base::RefCounted<FrameBase>, GBAFrame>);
static_assert(!std::is_base_of_v<kiwi::base::RefCounted<GBAFrame>, GBAFrame>);

TEST(GBAFrameTest, PixelFormatMatchesMGBAColorLayout) {
  int bits_per_pixel = 0;
  Uint32 red_mask = 0;
  Uint32 green_mask = 0;
  Uint32 blue_mask = 0;
  Uint32 alpha_mask = 0;

  ASSERT_TRUE(SDL_PixelFormatEnumToMasks(GBAFrame::kPixelFormat,
                                         &bits_per_pixel, &red_mask,
                                         &green_mask, &blue_mask, &alpha_mask));
  EXPECT_EQ(bits_per_pixel, 32);
  EXPECT_EQ(red_mask, 0x000000ffu);
  EXPECT_EQ(green_mask, 0x0000ff00u);
  EXPECT_EQ(blue_mask, 0x00ff0000u);
  EXPECT_EQ(alpha_mask, 0xff000000u);
}
