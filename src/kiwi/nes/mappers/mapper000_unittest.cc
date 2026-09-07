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

class Mapper000Test : public MapperTest {};

TEST_F(Mapper000Test, MirrorsSinglePRGBank) {
  auto cartridge = LoadMapper(0, 1, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(0x3fff));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x3fff));
}

TEST_F(Mapper000Test, MapsTwoPRGBanksAndReadOnlyCHRROM) {
  auto cartridge = LoadMapper(0, 2, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));
  const Byte original = mapper->ReadCHR(0x1234);
  mapper->WriteCHR(0x1234, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x1234), original);
}

TEST_F(Mapper000Test, PersistsCHRRAMThroughStateRoundTrip) {
  auto cartridge = LoadMapper(0, 2, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0000, 0x12);
  mapper->WriteCHR(0x1fff, 0x34);
  const Bytes state = SerializeMapper(mapper);
  mapper->WriteCHR(0x0000, 0xaa);
  mapper->WriteCHR(0x1fff, 0xbb);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0000), 0x12);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x34);
}

TEST_F(Mapper000Test, ReadOnlyWritesDoNotChangeMappedData) {
  auto cartridge = LoadMapper(0, 2, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original_prg = mapper->ReadPRG(0x8123);
  mapper->WritePRG(0x8123, original_prg ^ 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), original_prg);

  const Byte original_chr = mapper->ReadCHR(0x0123);
  mapper->WriteCHR(0x0123, original_chr ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0123), original_chr);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
