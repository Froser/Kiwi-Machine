// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "nes/mappers/mapper_test_support.h"

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k1K = 0x0400;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper119Test : public MapperTest {};

TEST_F(Mapper119Test, CreatesTQROMWithInitialMMC3Layout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(119));
  auto cartridge = LoadMapper(119, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_FALSE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(15 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0155), TestCHRByte(0x0155));
}

TEST_F(Mapper119Test, SelectsCHRROMWhenBankBitSixIsClear) {
  auto cartridge = LoadMapper(119, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 2, 0x3f);
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(63 * k1K + 0x155));

  WriteMMC3Register(mapper, 2, 0x80);
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(0x155));
}

TEST_F(Mapper119Test, SelectsAndMirrorsEightKiBOfCHRRAM) {
  auto cartridge = LoadMapper(119, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 2, 0x45);
  mapper->WriteCHR(0x1123, 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x5a);

  WriteMMC3Register(mapper, 2, 0xc5);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x5a);

  WriteMMC3Register(mapper, 2, 0x4d);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x5a);

  WriteMMC3Register(mapper, 2, 0x46);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0);
}

TEST_F(Mapper119Test, MapsTwoKiBCHRRAMBanksInBothCHRModeLayouts) {
  auto cartridge = LoadMapper(119, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 0, 0x44);
  mapper->WriteCHR(0x0123, 0x34);
  mapper->WriteCHR(0x0523, 0x35);

  WriteMMC3Register(mapper, 2, 0x46);
  mapper->WriteCHR(0x1123, 0x36);

  mapper->WritePRG(0x8000, 0x80);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x36);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x34);
  EXPECT_EQ(mapper->ReadCHR(0x1523), 0x35);
}

TEST_F(Mapper119Test, IgnoresWritesWhileCHRROMIsSelected) {
  auto cartridge = LoadMapper(119, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 2, 0x03);
  const Byte expected = TestCHRByte(3 * k1K + 0x155);
  EXPECT_EQ(mapper->ReadCHR(0x1155), expected);
  mapper->WriteCHR(0x1155, static_cast<Byte>(expected ^ 0xff));
  EXPECT_EQ(mapper->ReadCHR(0x1155), expected);
}

TEST_F(Mapper119Test, PreservesMMC3BehaviorAndMixedCHRState) {
  auto cartridge = LoadMapper(119, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 3);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  WriteMMC3Register(mapper, 2, 0x45);
  mapper->WriteCHR(0x1123, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  WriteMMC3Register(mapper, 6, 1);
  mapper->WritePRG(0xa000, 0);
  mapper->WritePRG(0xe000, 0);
  mapper->WriteCHR(0x1123, 0xd6);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x6d);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper119Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "High Speed (Europe).nes",
      "High Speed (USA).nes",
      "Pin-Bot (Europe).nes",
      "Pin-Bot (USA).nes",
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() ||
        !expected_roms.contains(entry.path().filename().string())) {
      continue;
    }

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    ASSERT_EQ(emulator_->GetRomData()->mapper, 119) << entry.path();

    emulator_->Run();
    bool rendered = false;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      const std::set<Color> unique_colors(pixels.begin(), pixels.end());
      if (unique_colors.size() > 1)
        rendered = true;
    }
    EXPECT_TRUE(rendered) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, expected_roms.size());
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
