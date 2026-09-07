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

class Mapper002Test : public MapperTest {};

TEST_F(Mapper002Test, SwitchesLowerPRGBankAndKeepsLastBankFixed) {
  auto cartridge = LoadMapper(2, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * 0x4000));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(3 * 0x4000 + 0x3fff));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(7 * 0x4000));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(8 * 0x4000 - 1));
}

TEST_F(Mapper002Test, IgnoresWritesBelowBankRegisterRange) {
  auto cartridge = LoadMapper(2, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 2);
  mapper->WriteExtendedRAM(0x5000, 5);

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * 0x4000));
}

TEST_F(Mapper002Test, PersistsBankAndCHRRAM) {
  auto cartridge = LoadMapper(2, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 4);
  mapper->WriteCHR(0x1fff, 0x5a);
  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0x8000, 1);
  mapper->WriteCHR(0x1fff, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(4 * 0x4000));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x5a);
}

TEST_F(Mapper002Test, LeavesCHRROMReadOnly) {
  auto cartridge = LoadMapper(2, 8, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original = mapper->ReadCHR(0x1234);
  mapper->WriteCHR(0x1234, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x1234), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
