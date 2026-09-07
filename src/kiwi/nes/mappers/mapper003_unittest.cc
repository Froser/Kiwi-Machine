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

constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper003Test : public MapperTest {};

TEST_F(Mapper003Test, MirrorsPRGAndClampsCHRBank) {
  auto one_bank = LoadMapper(3, 1, 3);
  ASSERT_TRUE(one_bank);
  EXPECT_EQ(one_bank->mapper()->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(one_bank->mapper()->ReadPRG(0xc000), TestPRGByte(0));

  auto two_banks = LoadMapper(3, 2, 3);
  ASSERT_TRUE(two_banks);
  Mapper* mapper = two_banks->mapper();
  mapper->WritePRG(0x7fff, 2);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0));
  mapper->WritePRG(0x8000, 2);
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(2 * 0x2000));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), TestCHRByte(3 * 0x2000 - 1));
  mapper->WritePRG(0x8000, 3);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0));
}

TEST_F(Mapper003Test, PersistsSelectedCHRBank) {
  auto cartridge = LoadMapper(3, 2, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 2);
  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0x8000, 1);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * 0x2000 + 0x0456));
}

TEST_F(Mapper003Test, WarnsForNonzeroSubmapperWithoutChangingMapping) {
  auto cartridge = LoadMapper(3, 2, 2, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 1);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(k8K));
}

TEST_F(Mapper003Test, ReadOnlyWritesDoNotChangeMappedData) {
  auto cartridge = LoadMapper(3, 2, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original = mapper->ReadCHR(0x0456);
  mapper->WriteCHR(0x0456, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0456), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
