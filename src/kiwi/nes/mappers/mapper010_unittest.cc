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

constexpr size_t k4K = 0x1000;
constexpr size_t k16K = 0x4000;

}  // namespace

class Mapper010Test : public MapperTest {};

TEST_F(Mapper010Test, DelaysSecondCHRBankLatchByOneTile) {
  auto cartridge = LoadMapper(10, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xe000, 5);
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(5 * k4K));
  mapper->WritePRG(0xd000, 4);

  EXPECT_EQ(mapper->ReadCHR(0x1fd0), TestCHRByte(5 * k4K + 0x0fd0));
  for (int read = 0; read < 15; ++read)
    EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(5 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(4 * k4K));
}

TEST_F(Mapper010Test, SwitchesPRGAndPersistsDelayedState) {
  auto cartridge = LoadMapper(10, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa000, 3);
  mapper->WritePRG(0xd000, 4);
  mapper->ReadCHR(0x1fd0);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xa000, 1);
  for (int read = 0; read < 16; ++read)
    mapper->ReadCHR(0x1000);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k16K));
  for (int read = 0; read < 15; ++read)
    mapper->ReadCHR(0x1000);
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(4 * k4K));
}

TEST_F(Mapper010Test, UpdatesBothLatchesMirroringAndFixedPRG) {
  auto cartridge = LoadMapper(10, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xb000, 2);
  mapper->WritePRG(0xc000, 3);
  EXPECT_EQ(mapper->ReadCHR(0x0fd0), TestCHRByte(2 * k4K + 0x0fd0));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x0fe0), TestCHRByte(3 * k4K + 0x0fe0));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(3 * k4K));

  mapper->WritePRG(0xd000, 4);
  mapper->WritePRG(0xe000, 5);
  mapper->ReadCHR(0x1fd0);
  for (int read = 0; read < 16; ++read)
    mapper->ReadCHR(0x1000);
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(4 * k4K));
  mapper->ReadCHR(0x1fe0);
  for (int read = 0; read < 16; ++read)
    mapper->ReadCHR(0x1000);
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(5 * k4K));

  mapper->WritePRG(0xf000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xf000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(8 * k16K - 1));

  const Byte original = mapper->ReadCHR(0x0123);
  mapper->WriteCHR(0x0123, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0123), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
