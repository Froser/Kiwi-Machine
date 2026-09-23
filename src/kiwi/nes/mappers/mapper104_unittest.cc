// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k16K = 0x4000;

}  // namespace

class Mapper104Test : public MapperTest {};

TEST_F(Mapper104Test, MapsInnerAndOuterPRGBanks) {
  auto cartridge = LoadMapper(104, 80, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(15 * k16K));

  mapper->WritePRG(0xc000, 0x0b);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(11 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(15 * k16K));

  mapper->WritePRG(0x8000, 0x0c);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(75 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(79 * k16K));

  mapper->WritePRG(0x8000, 0x02);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(75 * k16K));
}

TEST_F(Mapper104Test, SoftwareResetPreservesSelectedGame) {
  auto cartridge = LoadMapper(104, 80, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc000, 3);
  mapper->WritePRG(0x8000, 0x0a);
  mapper->Reset();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(35 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(47 * k16K));
}

TEST_F(Mapper104Test, PersistsBankAndCHRRAM) {
  auto cartridge = LoadMapper(104, 80, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc000, 4);
  mapper->WritePRG(0x8000, 0x09);
  mapper->WriteCHR(0x1fff, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0x8000, 0x0c);
  mapper->WriteCHR(0x1fff, 0xa5);
  ASSERT_TRUE(DeserializeMapper(mapper, state));

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(20 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(31 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x5a);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
