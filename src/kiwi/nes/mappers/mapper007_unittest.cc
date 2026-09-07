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

class Mapper007Test : public MapperTest {};

TEST_F(Mapper007Test, SwitchesPRGAndOneScreenMirroring) {
  auto cartridge = LoadMapper(7, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x6000, 0x17);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(7 * 0x8000));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(8 * 0x8000 - 1));
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
  EXPECT_EQ(mirroring_changes_, 1);

  mapper->WriteCHR(0x1fff, 0x6c);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x6c);
}

TEST_F(Mapper007Test, PersistsBankMirroringAndCHRRAM) {
  auto cartridge = LoadMapper(7, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x12);
  mapper->WriteCHR(0x0123, 0x45);
  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0x8000, 0x01);
  mapper->WriteCHR(0x0123, 0x67);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * 0x8000));
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
}

TEST_F(Mapper007Test, LeavesCHRROMReadOnly) {
  auto cartridge = LoadMapper(7, 16, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original = mapper->ReadCHR(0x1234);
  mapper->WriteCHR(0x1234, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x1234), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
