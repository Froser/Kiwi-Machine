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

class Mapper011Test : public MapperTest {};

TEST_F(Mapper011Test, SwitchesPRGAndCHRWithOneRegister) {
  auto cartridge = LoadMapper(11, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0xb2);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * 0x8000));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(3 * 0x8000 - 1));
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(11 * 0x2000));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), TestCHRByte(12 * 0x2000 - 1));

  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0x8000, 0x10);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * 0x8000));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(11 * 0x2000));
}

TEST_F(Mapper011Test, ReadOnlyWritesDoNotChangeMappedData) {
  auto cartridge = LoadMapper(11, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original = mapper->ReadCHR(0x0789);
  mapper->WriteCHR(0x0789, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0789), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
