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

class Mapper087Test : public MapperTest {};

TEST_F(Mapper087Test, UsesReversedCHRBankBits) {
  auto cartridge = LoadMapper(87, 2, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x6000, 0x01);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * 0x2000));
  mapper->WritePRG(0x6000, 0x02);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(1 * 0x2000));
  mapper->WritePRG(0x6000, 0x03);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), TestCHRByte(4 * 0x2000 - 1));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));

  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0x6000, 0);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(3 * 0x2000));
}

TEST_F(Mapper087Test, RejectsWritesOutsideItsRegisterRange) {
  auto cartridge = LoadMapper(87, 2, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x6000, 1);
  mapper->WritePRG(0x5fff, 2);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k8K));

  const Byte original = mapper->ReadCHR(0x0123);
  mapper->WriteCHR(0x0123, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0123), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
