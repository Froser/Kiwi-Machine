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

class Mapper075Test : public MapperTest {};

TEST_F(Mapper075Test, MapsPRGCHRAndMirroringRegisters) {
  auto cartridge = LoadMapper(75, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 2);
  mapper->WritePRG(0xa000, 3);
  mapper->WritePRG(0xc000, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0x9fff), TestPRGByte(3 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(4 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xdfff), TestPRGByte(5 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(15 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(16 * k8K - 1));

  mapper->WritePRG(0xe000, 3);
  mapper->WritePRG(0xf000, 5);
  mapper->WritePRG(0x9000, 0x07);
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(19 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(21 * k4K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mirroring_changes_, 1);
}

TEST_F(Mapper075Test, PersistsAllRegisters) {
  auto cartridge = LoadMapper(75, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 2);
  mapper->WritePRG(0xa000, 3);
  mapper->WritePRG(0xc000, 4);
  mapper->WritePRG(0xe000, 5);
  mapper->WritePRG(0xf000, 6);
  mapper->WritePRG(0x9000, 1);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 0);
  mapper->WritePRG(0xe000, 0);
  mapper->WritePRG(0x9000, 0);
  ASSERT_TRUE(DeserializeMapper(mapper, state));

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(5 * k4K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper075Test, ReadOnlyWritesDoNotChangeMappedData) {
  auto cartridge = LoadMapper(75, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original = mapper->ReadCHR(0x0def);
  mapper->WriteCHR(0x0def, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0def), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
