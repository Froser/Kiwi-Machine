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
constexpr size_t k2K = 0x0800;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper048Test : public MapperTest {};

TEST_F(Mapper048Test, MapsAllPRGAndCHRWindows) {
  auto cartridge = LoadMapper(48, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0x8001, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(5 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(16 * k8K - 1));

  mapper->WritePRG(0x8002, 2);
  mapper->WritePRG(0x8003, 3);
  mapper->WritePRG(0xa000, 8);
  mapper->WritePRG(0xa001, 9);
  mapper->WritePRG(0xa002, 10);
  mapper->WritePRG(0xa003, 11);
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(2 * k2K));
  EXPECT_EQ(mapper->ReadCHR(0x07ff), TestCHRByte(3 * k2K - 1));
  EXPECT_EQ(mapper->ReadCHR(0x0800), TestCHRByte(3 * k2K));
  EXPECT_EQ(mapper->ReadCHR(0x0fff), TestCHRByte(4 * k2K - 1));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(8 * k1K));
  EXPECT_EQ(mapper->ReadCHR(0x1400), TestCHRByte(9 * k1K));
  EXPECT_EQ(mapper->ReadCHR(0x1800), TestCHRByte(10 * k1K));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), TestCHRByte(12 * k1K - 1));
}

TEST_F(Mapper048Test, ControlsMirroringAndOneShotIRQ) {
  auto cartridge = LoadMapper(48, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xe000, 0x00);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xe000, 0x40);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mirroring_changes_, 2);

  mapper->WritePRG(0xc000, 0xfe);
  mapper->WritePRG(0xc002, 0);
  mapper->ScanlineIRQ(-1, true);
  mapper->ScanlineIRQ(0, false);
  mapper->ScanlineIRQ(0, true);
  EXPECT_EQ(irq_count_, 0);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);
  mapper->ScanlineIRQ(2, true);
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0xc001, 0xff);
  mapper->WritePRG(0xc002, 0);
  mapper->WritePRG(0xc003, 0);
  mapper->ScanlineIRQ(3, true);
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper048Test, ResetAndStateRoundTripRestoreRegisters) {
  auto cartridge = LoadMapper(48, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0x8002, 5);
  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0x8002, 2);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(5 * k2K));

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0));
}

TEST_F(Mapper048Test, IgnoresCHRROMWrites) {
  auto cartridge = LoadMapper(48, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original = mapper->ReadCHR(0x0123);
  mapper->WriteCHR(0x0123, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0123), original);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
