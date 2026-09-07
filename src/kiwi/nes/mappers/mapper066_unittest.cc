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

class Mapper066Test : public MapperTest {};

TEST_F(Mapper066Test, SwitchesPRGAndCHRAndPersistsState) {
  auto cartridge = LoadMapper(66, 8, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(0));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), TestCHRByte(0x1fff));

  mapper->WritePRG(0x8000, 0x31);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * 0x8000));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(4 * 0x8000 - 1));
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(0x2000));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), TestCHRByte(0x3fff));

  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0x8000, 0x02);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * 0x8000));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0x2000));
}

TEST_F(Mapper066Test, ReadOnlyWritesDoNotChangeMappedData) {
  auto cartridge = LoadMapper(66, 8, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  mapper->WritePRG(0x8000, 0);

  const Byte original = mapper->ReadCHR(0x0abc);
  mapper->WriteCHR(0x0abc, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0abc), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
