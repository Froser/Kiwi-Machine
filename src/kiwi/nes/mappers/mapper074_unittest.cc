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

constexpr size_t k1K = 0x0400;

}  // namespace

class Mapper074Test : public MapperTest {};

TEST_F(Mapper074Test, UsesRAMOnlyForBanksEightAndNine) {
  auto cartridge = LoadMapper(74, 8, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 2, 8);
  mapper->WriteCHR(0x1000, 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x1000), 0x5a);

  WriteMMC3Register(mapper, 2, 7);
  mapper->WriteCHR(0x1001, 0xa5);
  EXPECT_EQ(mapper->ReadCHR(0x1001), TestCHRByte(7 * k1K + 1));

  WriteMMC3Register(mapper, 2, 8);
  EXPECT_EQ(mapper->ReadCHR(0x1001), 0);
}

TEST_F(Mapper074Test, PersistsMixedCHRRAM) {
  auto cartridge = LoadMapper(74, 8, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 3, 9);
  mapper->WriteCHR(0x1400, 0x3c);
  const Bytes state = SerializeMapper(mapper);
  mapper->WriteCHR(0x1400, 0xc3);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x1400), 0x3c);
}

TEST_F(Mapper074Test, FallsBackToCHRROMForOtherBanks) {
  auto cartridge = LoadMapper(74, 8, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 2, 7);
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(7 * k1K));
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
