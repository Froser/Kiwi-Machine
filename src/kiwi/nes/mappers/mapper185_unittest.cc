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

class Mapper185Test : public MapperTest {};

TEST_F(Mapper185Test, UsesCNROMBankingBehavior) {
  auto cartridge = LoadMapper(185, 2, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(3 * 0x2000));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), TestCHRByte(4 * 0x2000 - 1));
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
