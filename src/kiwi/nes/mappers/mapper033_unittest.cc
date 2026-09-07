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

class Mapper033Test : public MapperTest {};

TEST_F(Mapper033Test, UsesRegister8000ForMirroringAndHasNoIRQ) {
  auto cartridge = LoadMapper(33, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x43);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0x8000, 0x02);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->WritePRG(0xc000, 0xff);
  mapper->WritePRG(0xc002, 0);
  mapper->ScanlineIRQ(0, true);
  EXPECT_EQ(irq_count_, 0);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
