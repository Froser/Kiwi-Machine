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
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper009Test : public MapperTest {};

TEST_F(Mapper009Test, SwitchesPRGCHRAndMirroring) {
  auto cartridge = LoadMapper(9, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa000, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(5 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0x9fff), TestPRGByte(6 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(13 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(14 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xdfff), TestPRGByte(15 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(15 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(16 * k8K - 1));

  mapper->WritePRG(0xb000, 2);
  mapper->WritePRG(0xc000, 3);
  mapper->WritePRG(0xd000, 4);
  mapper->WritePRG(0xe000, 5);
  EXPECT_EQ(mapper->ReadCHR(0x0fe0), TestCHRByte(3 * k4K + 0x0fe0));
  EXPECT_EQ(mapper->ReadCHR(0x0fd0), TestCHRByte(2 * k4K + 0x0fd0));
  EXPECT_EQ(mapper->ReadCHR(0x1fe0), TestCHRByte(5 * k4K + 0x0fe0));
  EXPECT_EQ(mapper->ReadCHR(0x1fd0), TestCHRByte(4 * k4K + 0x0fd0));

  mapper->WritePRG(0xf000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xf000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mirroring_changes_, 2);
}

TEST_F(Mapper009Test, PersistsLatchAndBankState) {
  auto cartridge = LoadMapper(9, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xb000, 2);
  mapper->ReadCHR(0x0fd0);
  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0xb000, 4);
  mapper->ReadCHR(0x0fd0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(2 * k4K));
}

TEST_F(Mapper009Test, UpdatesBothLatchesInBothDirections) {
  auto cartridge = LoadMapper(9, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xb000, 2);
  mapper->WritePRG(0xc000, 3);
  mapper->WritePRG(0xd000, 4);
  mapper->WritePRG(0xe000, 5);

  mapper->ReadCHR(0x0fd0);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k4K));
  mapper->ReadCHR(0x0fe0);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(3 * k4K));
  mapper->ReadCHR(0x1fd0);
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(4 * k4K));
  mapper->ReadCHR(0x1fe0);
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(5 * k4K));

  const Byte original = mapper->ReadCHR(0x0123);
  mapper->WriteCHR(0x0123, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0123), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
