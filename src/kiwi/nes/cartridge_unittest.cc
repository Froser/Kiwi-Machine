// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include "base/memory/ref_counted.h"
#include "nes/cartridge.h"
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {

class CartridgeHeaderTest : public MapperTest {};

TEST_F(CartridgeHeaderTest, IgnoresUnofficialINESByte10Garbage) {
  Bytes rom = MakeTestROM(0, 2, 1);
  rom[10] = 0x73;

  auto cartridge = base::MakeRefCounted<Cartridge>(
      static_cast<EmulatorImpl*>(emulator_.get()));
  EXPECT_TRUE(cartridge->Load(rom).success);
}

TEST_F(CartridgeHeaderTest, RejectsPALFromStandardINESByte9) {
  Bytes rom = MakeTestROM(0, 2, 1);
  rom[9] = 0x01;

  auto cartridge = base::MakeRefCounted<Cartridge>(
      static_cast<EmulatorImpl*>(emulator_.get()));
  EXPECT_FALSE(cartridge->Load(rom).success);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
