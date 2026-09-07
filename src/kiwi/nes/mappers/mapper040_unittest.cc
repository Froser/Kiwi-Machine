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

class Mapper040Test : public MapperTest {};

TEST_F(Mapper040Test, MapsFixedAndSwitchablePRGBanks) {
  auto cartridge = LoadMapper(40, 4, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), TestPRGByte(6 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(5 * k8K));
  mapper->WritePRG(0xe000, 2);
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(2 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xdfff), TestPRGByte(3 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(7 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(8 * k8K - 1));
}

TEST_F(Mapper040Test, SupportsCHRRAMAndCHRROM) {
  auto ram_cartridge = LoadMapper(40, 4, 0);
  ASSERT_TRUE(ram_cartridge);
  ram_cartridge->mapper()->WriteCHR(0x1fff, 0x5a);
  EXPECT_EQ(ram_cartridge->mapper()->ReadCHR(0x1fff), 0x5a);

  auto rom_cartridge = LoadMapper(40, 4, 1);
  ASSERT_TRUE(rom_cartridge);
  const Byte original = rom_cartridge->mapper()->ReadCHR(0x1234);
  rom_cartridge->mapper()->WriteCHR(0x1234, original ^ 0xff);
  EXPECT_EQ(rom_cartridge->mapper()->ReadCHR(0x1234), original);
}

TEST_F(Mapper040Test, RaisesIRQAfter4096M2Cycles) {
  auto cartridge = LoadMapper(40, 4, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  EXPECT_TRUE(mapper->NeedsM2CycleIRQ());

  mapper->WritePRG(0xa000, 0);
  for (int cycle = 0; cycle < 4095; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0x8000, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper040Test, PersistsBankIRQAndCHRRAMState) {
  auto cartridge = LoadMapper(40, 4, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xe000, 3);
  mapper->WritePRG(0xa000, 0);
  mapper->M2CycleIRQ();
  mapper->WriteCHR(0x0123, 0x45);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xe000, 1);
  mapper->WritePRG(0x8000, 0);
  mapper->WriteCHR(0x0123, 0x67);
  ASSERT_TRUE(DeserializeMapper(mapper, state));

  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
  for (int cycle = 1; cycle < 4096; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper040Test, StateRoundTripRestoresCounterAndBank) {
  auto cartridge = LoadMapper(40, 4, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  EXPECT_TRUE(mapper->NeedsM2CycleIRQ());

  mapper->WritePRG(0xe000, 3);
  mapper->WritePRG(0xa000, 0);
  mapper->M2CycleIRQ();
  mapper->WriteCHR(0x0123, 0x45);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xe000, 1);
  mapper->WritePRG(0x8000, 0);
  mapper->WriteCHR(0x0123, 0x67);
  ASSERT_TRUE(DeserializeMapper(mapper, state));

  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
