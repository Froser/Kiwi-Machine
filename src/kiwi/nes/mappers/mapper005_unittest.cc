// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <array>

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k1K = 0x0400;
constexpr size_t k4K = 0x1000;
constexpr size_t k8K = 0x2000;
constexpr size_t k16K = 0x4000;
constexpr size_t k32K = 0x8000;

void ActivateMMC5PRGMode(Mapper* mapper, Byte mode) {
  mapper->WriteExtendedRAM(0x5100, mode);
  mapper->WriteExtendedRAM(0x5117, 0x87);
  mapper->ReadPRG(0xe000);
}

}  // namespace

class Mapper005Test : public MapperTest {};

TEST_F(Mapper005Test, MapsAllPRGModes) {
  auto cartridge = LoadMapper(5, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5100, 0);
  mapper->WriteExtendedRAM(0x5117, 0x8c);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k32K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(4 * k32K - 1));

  mapper->WriteExtendedRAM(0x5100, 1);
  mapper->WriteExtendedRAM(0x5115, 0x84);
  mapper->WriteExtendedRAM(0x5117, 0x86);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k16K));

  mapper->WriteExtendedRAM(0x5100, 2);
  mapper->WriteExtendedRAM(0x5115, 0x84);
  mapper->WriteExtendedRAM(0x5116, 0x85);
  mapper->WriteExtendedRAM(0x5117, 0x86);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(5 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xdfff), TestPRGByte(6 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(6 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(7 * k8K - 1));

  mapper->WriteExtendedRAM(0x5100, 3);
  mapper->WriteExtendedRAM(0x5114, 0x81);
  mapper->WriteExtendedRAM(0x5115, 0x82);
  mapper->WriteExtendedRAM(0x5116, 0x83);
  mapper->WriteExtendedRAM(0x5117, 0x84);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(1 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0x9fff), TestPRGByte(2 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(2 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(3 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xdfff), TestPRGByte(4 * k8K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(5 * k8K - 1));
}

TEST_F(Mapper005Test, MapsCHRAtEverySupportedGranularity) {
  auto cartridge = LoadMapper(5, 16, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  mapper->SetCurrentRenderState(true, false, 0);

  mapper->WriteExtendedRAM(0x5101, 0);
  mapper->WriteExtendedRAM(0x5127, 8);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(8 * k1K));

  mapper->WriteExtendedRAM(0x5101, 1);
  mapper->WriteExtendedRAM(0x5123, 4);
  mapper->WriteExtendedRAM(0x5127, 12);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(4 * k1K));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(12 * k1K));

  mapper->WriteExtendedRAM(0x5101, 2);
  mapper->WriteExtendedRAM(0x5121, 2);
  mapper->WriteExtendedRAM(0x5123, 6);
  mapper->WriteExtendedRAM(0x5125, 10);
  mapper->WriteExtendedRAM(0x5127, 14);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k1K));
  EXPECT_EQ(mapper->ReadCHR(0x0800), TestCHRByte(6 * k1K));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(10 * k1K));
  EXPECT_EQ(mapper->ReadCHR(0x1800), TestCHRByte(14 * k1K));

  mapper->WriteExtendedRAM(0x5101, 3);
  for (Address reg = 0x5120; reg <= 0x5127; ++reg)
    mapper->WriteExtendedRAM(reg, static_cast<Byte>(reg - 0x5120 + 16));
  for (size_t page = 0; page < 8; ++page) {
    EXPECT_EQ(mapper->ReadCHR(static_cast<Address>(page * k1K)),
              TestCHRByte((page + 16) * k1K));
  }
}

TEST_F(Mapper005Test, ReadsAndWritesSRAMFromFirstAddress) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5102, 0x02);
  mapper->WriteExtendedRAM(0x5103, 0x01);
  mapper->WriteExtendedRAM(0x5113, 0);
  mapper->WriteExtendedRAM(0x6000, 0x4a);
  mapper->WriteExtendedRAM(0x7fff, 0x5b);

  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x4a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7fff), 0x5b);
}

TEST_F(Mapper005Test, WritesOnlyTheSelectedPRGRAMWindow) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5102, 0x02);
  mapper->WriteExtendedRAM(0x5103, 0x01);
  mapper->WriteExtendedRAM(0x5100, 3);
  mapper->WriteExtendedRAM(0x5114, 0);
  mapper->WriteExtendedRAM(0x5115, 2);
  mapper->WriteExtendedRAM(0x5116, 4);
  mapper->WriteExtendedRAM(0x5117, 0x87);
  mapper->ReadPRG(0xe000);
  mapper->WritePRG(0x8001, 0x6d);

  mapper->WriteExtendedRAM(0x5113, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x6d);
  mapper->WriteExtendedRAM(0x5113, 1);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0);
  mapper->WriteExtendedRAM(0x5113, 2);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0);
}

TEST_F(Mapper005Test, ImplementsMultiplierAndExtendedRAMModes) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5205, 0x12);
  mapper->WriteExtendedRAM(0x5206, 0x34);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5205), 0xa8);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5206), 0x03);

  mapper->WriteExtendedRAM(0x5104, 2);
  mapper->WriteExtendedRAM(0x5c00, 0x5a);
  mapper->WriteExtendedRAM(0x5fff, 0xa5);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5c00), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5fff), 0xa5);

  mapper->WriteExtendedRAM(0x5104, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5c00), 0);
}

TEST_F(Mapper005Test, ProtectsInternalVRAMWhileRendering) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  std::array<Byte, 0x800> ram{};

  mapper->WriteExtendedRAM(0x5104, 0);
  mapper->WriteExtendedRAM(0x5105, 0xaa);
  mapper->WriteCHR(0x0123, 0x45);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2123), 0x45);
  mapper->WriteNametableByte(ram.data(), 0x2124, 0x56);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2124), 0x56);

  mapper->ScanlineIRQ(0, true);
  mapper->WriteCHR(0x0123, 0x67);
  mapper->WriteNametableByte(ram.data(), 0x2124, 0x78);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2123), 0x45);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2124), 0x56);
}

TEST_F(Mapper005Test, UsesExtendedAndSplitCHRBanking) {
  auto cartridge = LoadMapper(5, 16, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  std::array<Byte, 0x800> ram{};
  EXPECT_TRUE(mapper->IsMMC5());

  mapper->WriteExtendedRAM(0x5104, 2);
  mapper->WriteExtendedRAM(0x5c00, 0x43);
  mapper->WriteExtendedRAM(0x5104, 1);
  mapper->SetCurrentRenderState(true, false, 0);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2000), 0);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(3 * k4K + 0x0123));

  mapper->WriteExtendedRAM(0x5104, 0);
  mapper->WriteExtendedRAM(0x5200, 0x80 | 4);
  mapper->WriteExtendedRAM(0x5202, 5);
  mapper->SetCurrentRenderState(true, false, 8);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(5 * k4K + 0x0123));
}

TEST_F(Mapper005Test, SelectsCIRAMExtendedRAMAndFillNametables) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  std::array<Byte, 0x800> ram{};
  ram[0x012] = 0x11;
  ram[0x412] = 0x22;

  mapper->WriteExtendedRAM(0x5104, 2);
  mapper->WriteExtendedRAM(0x5c12, 0x33);
  mapper->WriteExtendedRAM(0x5104, 0);
  mapper->WriteExtendedRAM(0x5105, 0xe4);
  mapper->WriteExtendedRAM(0x5106, 0x44);
  mapper->WriteExtendedRAM(0x5107, 0x02);

  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2012), 0x11);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2412), 0x22);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2812), 0x33);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2c12), 0x44);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2fc0), 0xaa);

  mapper->WriteNametableByte(ram.data(), 0x2013, 0x55);
  mapper->WriteNametableByte(ram.data(), 0x2413, 0x66);
  EXPECT_EQ(ram[0x013], 0x55);
  EXPECT_EQ(ram[0x413], 0x66);
}

TEST_F(Mapper005Test, TracksSplitRegionAndIRQ) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5200, 0x80 | 4);
  mapper->WriteExtendedRAM(0x5201, 3);
  mapper->SetCurrentRenderState(true, false, 16);
  mapper->ScanlineIRQ(-1, true);
  EXPECT_EQ(mapper->GetFineXInSplitRegion(7), 0);
  EXPECT_NE(mapper->GetDataAddressInSplitRegion(0x2000), 0x2000);

  mapper->SetCurrentRenderState(true, false, 40);
  EXPECT_EQ(mapper->GetFineXInSplitRegion(7), 7);
  EXPECT_EQ(mapper->GetDataAddressInSplitRegion(0x2345), 0x2345);

  mapper->WriteExtendedRAM(0x5203, 2);
  mapper->WriteExtendedRAM(0x5204, 0x80);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);
  EXPECT_NE(mapper->ReadExtendedRAM(0x5204) & 0x80, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5204) & 0x80, 0);
}

TEST_F(Mapper005Test, PersistsObservableState) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5105, 0xff);
  mapper->WriteExtendedRAM(0x5106, 0x42);
  mapper->WriteExtendedRAM(0x5205, 7);
  mapper->WriteExtendedRAM(0x5206, 9);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x5106, 0x11);
  mapper->WriteExtendedRAM(0x5205, 1);
  mapper->WriteExtendedRAM(0x5206, 1);
  ASSERT_TRUE(DeserializeMapper(mapper, state));

  std::array<Byte, 0x800> ram{};
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2000), 0x42);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5205), 63);
}

TEST_F(Mapper005Test, WritesEachValidPRGRAMWindow) {
  auto mode1_cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(mode1_cartridge);
  Mapper* mode1 = mode1_cartridge->mapper();
  mode1->WriteExtendedRAM(0x5102, 0x02);
  mode1->WriteExtendedRAM(0x5103, 0x01);
  ActivateMMC5PRGMode(mode1, 1);
  mode1->WriteExtendedRAM(0x5115, 2);
  mode1->WritePRG(0x8001, 0x11);
  mode1->WriteExtendedRAM(0x5113, 4);
  EXPECT_EQ(mode1->ReadExtendedRAM(0x6001), 0x11);

  auto mode2_cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(mode2_cartridge);
  Mapper* mode2 = mode2_cartridge->mapper();
  mode2->WriteExtendedRAM(0x5102, 0x02);
  mode2->WriteExtendedRAM(0x5103, 0x01);
  ActivateMMC5PRGMode(mode2, 2);
  mode2->WriteExtendedRAM(0x5116, 2);
  mode2->WritePRG(0xc001, 0x22);
  mode2->WriteExtendedRAM(0x5113, 2);
  EXPECT_EQ(mode2->ReadExtendedRAM(0x6001), 0x22);

  auto mode3_cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(mode3_cartridge);
  Mapper* mode3 = mode3_cartridge->mapper();
  mode3->WriteExtendedRAM(0x5102, 0x02);
  mode3->WriteExtendedRAM(0x5103, 0x01);
  ActivateMMC5PRGMode(mode3, 3);
  mode3->WriteExtendedRAM(0x5116, 3);
  mode3->WritePRG(0xc001, 0x33);
  mode3->WriteExtendedRAM(0x5113, 3);
  EXPECT_EQ(mode3->ReadExtendedRAM(0x6001), 0x33);
}

TEST_F(Mapper005Test, HonorsWriteProtectionAndDirectSRAMWindow) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5113, 2);
  mapper->WriteExtendedRAM(0x6001, 0x11);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0);

  mapper->WriteExtendedRAM(0x5102, 0x02);
  mapper->WriteExtendedRAM(0x5103, 0x01);
  mapper->WriteExtendedRAM(0x6001, 0x22);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x22);

  ActivateMMC5PRGMode(mapper, 0);
  mapper->WritePRG(0x8001, 0x33);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x22);
}

TEST_F(Mapper005Test, SelectsSpriteAndBackgroundCHRRegisters) {
  auto cartridge = LoadMapper(5, 16, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5101, 3);
  mapper->WriteExtendedRAM(0x5120, 2);
  mapper->WriteExtendedRAM(0x5128, 20);

  mapper->SetCurrentRenderState(false, true, 0);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k1K));
  mapper->SetCurrentRenderState(true, true, 0);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(20 * k1K));
}

TEST_F(Mapper005Test, ReadsExtendedAttributesAndSplitNametableData) {
  auto cartridge = LoadMapper(5, 16, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  std::array<Byte, 0x800> ram{};

  mapper->WriteExtendedRAM(0x5104, 2);
  mapper->WriteExtendedRAM(0x5c00, 0x43);
  mapper->WriteExtendedRAM(0x5104, 1);
  mapper->SetCurrentRenderState(true, false, 0);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2000), 0);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x23c0), 0x55);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(3 * k4K + 0x0123));

  mapper->WriteExtendedRAM(0x5104, 0);
  mapper->WriteExtendedRAM(0x5200, 0x80 | 4);
  mapper->SetCurrentRenderState(true, false, 8);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x2000), 0x43);
  EXPECT_EQ(mapper->ReadNametableByte(ram.data(), 0x23c0), 0);
}

TEST_F(Mapper005Test, HandlesRightSplitAndVerticalWrap) {
  auto cartridge = LoadMapper(5, 16, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5200, 0xc0 | 4);
  mapper->WriteExtendedRAM(0x5201, 0xef);
  mapper->WriteExtendedRAM(0x5202, 5);
  mapper->SetCurrentRenderState(true, false, 40);
  mapper->ScanlineIRQ(-1, true);
  EXPECT_EQ(mapper->GetFineXInSplitRegion(7), 0);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(5 * k4K));

  mapper->ScanlineIRQ(0, true);
  EXPECT_EQ(mapper->GetDataAddressInSplitRegion(0x2345), 4);

  mapper->SetCurrentRenderState(true, false, 16);
  EXPECT_EQ(mapper->GetFineXInSplitRegion(7), 7);

  mapper->WriteExtendedRAM(0x5200, 0x80 | 4);
  mapper->WriteExtendedRAM(0x5201, 0x07);
  mapper->SetCurrentRenderState(true, false, 8);
  mapper->ScanlineIRQ(-1, true);
  mapper->ScanlineIRQ(0, true);
  EXPECT_EQ(mapper->GetDataAddressInSplitRegion(0x2345), 0x0024);
}

TEST_F(Mapper005Test, ClearsFrameAndIRQFlagsOutsideRendering) {
  auto cartridge = LoadMapper(5, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5203, 1);
  mapper->WriteExtendedRAM(0x5204, 0x80);
  mapper->ScanlineIRQ(0, true);
  EXPECT_EQ(irq_count_, 1);
  mapper->ScanlineIRQ(240, false);
  mapper->ScanlineIRQ(241, false);
  mapper->ScanlineIRQ(242, false);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5204) & 0xc0, 0);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
