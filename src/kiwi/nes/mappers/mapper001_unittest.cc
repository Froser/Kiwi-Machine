// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <algorithm>
#include <array>

#include "base/functional/bind.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k4K = 0x1000;
constexpr size_t k8K = 0x2000;
constexpr size_t k16K = 0x4000;
constexpr size_t kINESHeaderSize = 0x10;

}  // namespace

class Mapper001Test : public MapperTest {};

TEST_F(Mapper001Test, ProvidesWorkRAMWithoutBatteryFlag) {
  auto cartridge = LoadMapper(1, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_TRUE(mapper->HasPRGRAM());
  EXPECT_FALSE(mapper->HasBatteryBackedRAM());
  EXPECT_FALSE(cartridge->GetRomData()->has_battery);
  EXPECT_EQ(cartridge->GetRomData()->prg_ram_size, k8K);
  EXPECT_EQ(cartridge->GetRomData()->prg_nvram_size, 0u);
  mapper->WriteExtendedRAM(0x6000, 0x5a);
  mapper->WriteExtendedRAM(0x7fff, 0xa5);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7fff), 0xa5);
}

TEST_F(Mapper001Test, DistinguishesBatteryBackedRAMFromWorkRAM) {
  auto cartridge = LoadMapper(1, 8, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_TRUE(mapper->HasPRGRAM());
  EXPECT_TRUE(mapper->HasBatteryBackedRAM());
  EXPECT_TRUE(cartridge->GetRomData()->has_battery);
  EXPECT_EQ(cartridge->GetRomData()->prg_ram_size, 0u);
  EXPECT_EQ(cartridge->GetRomData()->prg_nvram_size, k8K);
}

TEST_F(Mapper001Test, AllPRGModes) {
  struct TestCase {
    Byte control;
    Byte selected_bank;
    size_t lower_bank;
    size_t upper_bank;
  };
  constexpr std::array<TestCase, 4> kCases{{
      {0x00, 3, 2, 3},
      {0x04, 3, 2, 3},
      {0x08, 4, 0, 4},
      {0x0c, 4, 4, 7},
  }};

  for (const auto& test : kCases) {
    auto cartridge = LoadMapper(1, 8, 16);
    ASSERT_TRUE(cartridge);
    Mapper* mapper = cartridge->mapper();
    WriteMMC1Register(mapper, 0x8000, test.control);
    WriteMMC1Register(mapper, 0xe000, test.selected_bank);

    EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(test.lower_bank * k16K));
    EXPECT_EQ(mapper->ReadPRG(0xbfff),
              TestPRGByte((test.lower_bank + 1) * k16K - 1));
    EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(test.upper_bank * k16K));
    EXPECT_EQ(mapper->ReadPRG(0xffff),
              TestPRGByte((test.upper_bank + 1) * k16K - 1));
  }
}

TEST_F(Mapper001Test, MapsIndependentCHRAndMirroringModes) {
  auto cartridge = LoadMapper(1, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC1Register(mapper, 0x8000, 0x10);
  WriteMMC1Register(mapper, 0xa000, 2);
  WriteMMC1Register(mapper, 0xc000, 5);
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(2 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x0fff), TestCHRByte(3 * k4K - 1));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(5 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), TestCHRByte(6 * k4K - 1));

  constexpr std::array<NametableMirroring, 4> kMirroringModes{{
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
  }};
  for (Byte mode = 0; mode < 4; ++mode) {
    WriteMMC1Register(mapper, 0x8000, static_cast<Byte>(0x10 | mode));
    EXPECT_EQ(mapper->GetNametableMirroring(), kMirroringModes[mode]);
  }
  EXPECT_EQ(mirroring_changes_, 5);
}

TEST_F(Mapper001Test, IgnoresLowCHRBankBitInEightKilobyteMode) {
  auto cartridge = LoadMapper(1, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC1Register(mapper, 0x8000, 0x00);
  WriteMMC1Register(mapper, 0xa000, 3);

  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(2 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(3 * k4K));
}

TEST_F(Mapper001Test, ResetWriteRestoresLastFixedPRGMode) {
  auto cartridge = LoadMapper(1, 8, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC1Register(mapper, 0x8000, 0x08);
  WriteMMC1Register(mapper, 0xe000, 4);
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(4 * k16K));

  mapper->WritePRG(0x8000, 0x80);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(4 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(7 * k16K));
}

TEST_F(Mapper001Test, HandlesReadModifyWriteRegisterWrites) {
  Bytes rom = MakeTestROM(1, 8, 1);
  const size_t fixed_bank = kINESHeaderSize + 7 * k16K;
  constexpr std::array<Byte, 20> kProgram = {
      0xee, 0x23, 0xe1,  // INC $E123
      0xa9, 0x00,        // LDA #$00
      0x8d, 0x00, 0xe0,  // STA $E000
      0x8d, 0x00, 0xe0,  // STA $E000
      0x8d, 0x00, 0xe0,  // STA $E000
      0x8d, 0x00, 0xe0,  // STA $E000
      0x4c, 0x11, 0xc0,  // JMP $C011
  };
  std::copy(kProgram.begin(), kProgram.end(), rom.begin() + fixed_bank);
  rom[fixed_bank + 0x2123] = 0x00;
  rom[fixed_bank + 0x3ffa] = 0x11;
  rom[fixed_bank + 0x3ffb] = 0xc0;
  rom[fixed_bank + 0x3ffc] = 0x00;
  rom[fixed_bank + 0x3ffd] = 0xc0;
  rom[fixed_bank + 0x3ffe] = 0x11;
  rom[fixed_bank + 0x3fff] = 0xc0;

  bool loaded = false;
  emulator_->LoadFromBinary(
      rom, base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                          &loaded));
  ASSERT_TRUE(loaded);

  for (int cycle = 0; cycle < 30; ++cycle)
    emulator_->Step();

  DebugPort debug_port(emulator_.get());
  EXPECT_EQ(debug_port.GetCPUContext().registers.PC, 0xc011);
  EXPECT_EQ(debug_port.CPUReadByte(0x8000), TestPRGByte(0));
}

TEST_F(Mapper001Test, PersistsRegistersAndCHRRAM) {
  auto cartridge = LoadMapper(1, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC1Register(mapper, 0x8000, 0x1f);
  WriteMMC1Register(mapper, 0xe000, 4);
  mapper->WriteCHR(0x1fff, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  WriteMMC1Register(mapper, 0x8000, 0x0c);
  WriteMMC1Register(mapper, 0xe000, 1);
  mapper->WriteCHR(0x1fff, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(4 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x5a);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
