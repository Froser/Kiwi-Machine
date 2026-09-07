// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/ppu_bus.h"

#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace nes {
namespace testing {

TEST(PPUBusTest, MirrorsLastNametableAddress) {
  PPUBus bus;

  bus.Write(0x2eff, 0x5a);

  EXPECT_EQ(bus.Read(0x3eff), 0x5a);
}

TEST(PPUBusTest, ReadsAndWritesEntirePaletteRange) {
  PPUBus bus;

  bus.Write(0x3f01, 0x21);
  bus.Write(0x3fff, 0x7f);

  EXPECT_EQ(bus.Read(0x3f01), 0x21);
  EXPECT_EQ(bus.Read(0x3fff), 0x3f);
}

TEST(PPUBusTest, MirrorsUniversalBackgroundPaletteEntries) {
  PPUBus bus;

  bus.Write(0x3f00, 0x12);
  EXPECT_EQ(bus.Read(0x3f10), 0x12);

  bus.Write(0x3f1c, 0x23);
  EXPECT_EQ(bus.Read(0x3f0c), 0x23);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
