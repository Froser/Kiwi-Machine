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
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper004Test : public MapperTest {};

TEST_F(Mapper004Test, MapsEveryPRGWindowInBothModes) {
  auto cartridge = LoadMapper(4, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 3);
  WriteMMC3Register(mapper, 7, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(16 * k8K - 1));

  mapper->WritePRG(0x8000, 0x46);
  mapper->WritePRG(0x8001, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(15 * k8K));
}

TEST_F(Mapper004Test, MapsEveryCHRWindowInBothModes) {
  auto cartridge = LoadMapper(4, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  const std::array<Byte, 6> registers{{2, 4, 6, 7, 8, 9}};

  for (Byte i = 0; i < registers.size(); ++i)
    WriteMMC3Register(mapper, i, registers[i]);

  const std::array<size_t, 8> normal_banks{{2, 3, 4, 5, 6, 7, 8, 9}};
  for (size_t page = 0; page < normal_banks.size(); ++page) {
    const Address address = static_cast<Address>(page * k1K + 0x155);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(normal_banks[page] * k1K + 0x155));
  }

  mapper->WritePRG(0x8000, 0x80);
  const std::array<size_t, 8> inverted_banks{{6, 7, 8, 9, 2, 3, 4, 5}};
  for (size_t page = 0; page < inverted_banks.size(); ++page) {
    const Address address = static_cast<Address>(page * k1K + 0x2aa);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(inverted_banks[page] * k1K + 0x2aa));
  }
}

TEST_F(Mapper004Test, SupportsCHRRAMAndExtendedRAMBoundaries) {
  auto cartridge = LoadMapper(4, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0000, 0x12);
  mapper->WriteCHR(0x1fff, 0x34);
  EXPECT_EQ(mapper->ReadCHR(0x0000), 0x12);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x34);

  mapper->WriteExtendedRAM(0x6000, 0x56);
  mapper->WriteExtendedRAM(0x7fff, 0x78);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x56);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7fff), 0x78);
}

TEST_F(Mapper004Test, SupportsFourScreenAndCartridgeRAMWrites) {
  auto four_screen = LoadMapper(4, 8, 8, 0x08);
  ASSERT_TRUE(four_screen);
  Mapper* mapper = four_screen->mapper();
  mapper->WriteCHR(0x2000, 0x12);
  mapper->WriteCHR(0x2fff, 0x34);
  EXPECT_EQ(mapper->ReadCHR(0x2000), 0x12);
  EXPECT_EQ(mapper->ReadCHR(0x2fff), 0x34);

  auto with_ram = LoadMapper(4, 8, 8, 0x02);
  ASSERT_TRUE(with_ram);
  with_ram->mapper()->WriteExtendedRAM(0x6000, 0x56);
  with_ram->mapper()->WriteExtendedRAM(0x7fff, 0x78);
  EXPECT_EQ(with_ram->mapper()->ReadExtendedRAM(0x6000), 0x56);
  EXPECT_EQ(with_ram->mapper()->ReadExtendedRAM(0x7fff), 0x78);
}

TEST_F(Mapper004Test, UpdatesMirroringAndHonorsFourScreenMode) {
  auto cartridge = LoadMapper(4, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xa000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0xa001, 0);
  EXPECT_EQ(mirroring_changes_, 2);

  auto four_screen = LoadMapper(4, 8, 8, 0x08);
  ASSERT_TRUE(four_screen);
  four_screen->mapper()->WritePRG(0xa000, 1);
  EXPECT_EQ(four_screen->mapper()->GetNametableMirroring(),
            NametableMirroring::kFourScreen);
}

TEST_F(Mapper004Test, TriggersAndDisablesScanlineIRQ) {
  auto cartridge = LoadMapper(4, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc000, 2);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 0);
  mapper->ScanlineIRQ(2, true);
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0xe000, 0);
  mapper->ScanlineIRQ(3, true);
  mapper->ScanlineIRQ(240, true);
  mapper->ScanlineIRQ(4, false);
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper004Test, DetectsPPUA12RisingEdges) {
  auto cartridge = LoadMapper(4, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  mapper->PPUAddressChanged(0x0000);
  mapper->PPUAddressChanged(0x1000);
  mapper->PPUAddressChanged(0x1001);
  EXPECT_EQ(irq_count_, 0);
  mapper->PPUAddressChanged(0x0000);
  mapper->PPUAddressChanged(0x1000);
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper004Test, PersistsBankIRQAndRAMState) {
  auto cartridge = LoadMapper(4, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 3);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  const Bytes state = SerializeMapper(mapper);

  WriteMMC3Register(mapper, 6, 1);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
  mapper->WritePRG(0xe000, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper004Test, PersistsCHRRAMState) {
  auto cartridge = LoadMapper(4, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0123, 0x45);
  const Bytes state = SerializeMapper(mapper);
  mapper->WriteCHR(0x0123, 0x67);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
