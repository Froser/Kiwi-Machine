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

class Mapper000Test : public MapperTest {};

TEST_F(Mapper000Test, MirrorsSinglePRGBank) {
  auto cartridge = LoadMapper(0, 1, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(0x3fff));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x3fff));
}

TEST_F(Mapper000Test, MapsTwoPRGBanksAndReadOnlyCHRROM) {
  auto cartridge = LoadMapper(0, 2, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));
  const Byte original = mapper->ReadCHR(0x1234);
  mapper->WriteCHR(0x1234, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x1234), original);
}

TEST_F(Mapper000Test, PersistsCHRRAMThroughStateRoundTrip) {
  auto cartridge = LoadMapper(0, 2, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0000, 0x12);
  mapper->WriteCHR(0x1fff, 0x34);
  const Bytes state = SerializeMapper(mapper);
  mapper->WriteCHR(0x0000, 0xaa);
  mapper->WriteCHR(0x1fff, 0xbb);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0000), 0x12);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x34);
}

TEST_F(Mapper000Test, ReadOnlyWritesDoNotChangeMappedData) {
  auto cartridge = LoadMapper(0, 2, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original_prg = mapper->ReadPRG(0x8123);
  mapper->WritePRG(0x8123, original_prg ^ 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), original_prg);

  const Byte original_chr = mapper->ReadCHR(0x0123);
  mapper->WriteCHR(0x0123, original_chr ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0123), original_chr);
}

TEST_F(Mapper000Test, DistinguishesNoRAMFromBatteryBackedRAM) {
  auto no_ram = LoadMapper(0, 2, 1);
  ASSERT_TRUE(no_ram);
  EXPECT_FALSE(no_ram->mapper()->HasPRGRAM());
  EXPECT_FALSE(no_ram->mapper()->HasBatteryBackedRAM());
  EXPECT_EQ(no_ram->mapper()->ReadExtendedRAM(0x6000), 0x60);

  auto battery_ram = LoadMapper(0, 2, 1, 0x02);
  ASSERT_TRUE(battery_ram);
  EXPECT_TRUE(battery_ram->mapper()->HasPRGRAM());
  EXPECT_TRUE(battery_ram->mapper()->HasBatteryBackedRAM());
  EXPECT_TRUE(battery_ram->GetRomData()->has_battery);
  EXPECT_EQ(battery_ram->GetRomData()->prg_ram_size, 0u);
  EXPECT_EQ(battery_ram->GetRomData()->prg_nvram_size, k8K);
}

TEST_F(Mapper000Test, ReadsLegacyINESPRGRAMSize) {
  auto cartridge = LoadMapper(0, 2, 1, 0, 0, 2);
  ASSERT_TRUE(cartridge);

  EXPECT_FALSE(cartridge->GetRomData()->has_battery);
  EXPECT_EQ(cartridge->GetRomData()->prg_ram_size, 2u * k8K);
  EXPECT_EQ(cartridge->GetRomData()->prg_nvram_size, 0u);
}

TEST_F(Mapper000Test, ReadsNES20PRGRAMAndNVRAMSizes) {
  auto cartridge = LoadMapper(0, 2, 1, 0x02, 1, 0, 0x87);
  ASSERT_TRUE(cartridge);

  EXPECT_TRUE(cartridge->GetRomData()->is_nes_20);
  EXPECT_EQ(cartridge->GetRomData()->submapper, 1);
  EXPECT_TRUE(cartridge->GetRomData()->has_battery);
  EXPECT_EQ(cartridge->GetRomData()->prg_ram_size, k8K);
  EXPECT_EQ(cartridge->GetRomData()->prg_nvram_size, 2u * k8K);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
