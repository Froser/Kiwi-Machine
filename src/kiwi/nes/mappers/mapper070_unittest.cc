// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k16K = 0x4000;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper070Test : public MapperTest {};

TEST_F(Mapper070Test, MapsSelectedPRGAndCHRWithFixedLastPRGBank) {
  auto cartridge = LoadMapper(70, 16, 8, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0x83);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(8 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(15 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(3 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 3u * k8K + 0x0456);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper070Test, AppliesPRGROMBusConflictsToBankRegisterWrites) {
  auto cartridge = LoadMapper(70, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  ASSERT_EQ(mapper->ReadPRG(0x80f0), 0xf0);
  mapper->WritePRG(0x80f0, 0x3f);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0));
}

TEST_F(Mapper070Test, ControlsMapper152MirroringAndRestoresState) {
  auto cartridge = LoadMapper(152, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0xa5);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(5 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x80ff, 0x11);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(5 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
}

TEST_F(Mapper070Test, SupportsCHRRAMAndResetsRegisters) {
  auto cartridge = LoadMapper(70, 8, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0123, 0x6d);
  mapper->WritePRG(0x80ff, 0x30);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper070Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, Byte> expected_roms = {
      {"Arkanoid II (Japan).nes", 152},
      {"Family Trainer - Manhattan Police (Japan).nes", 70},
      {"Family Trainer - Meiro Daisakusen (Japan).nes", 70},
      {"Gegege no Kitarou 2 - Youkai Gundan no Chousen (Japan).nes", 152},
      {"Kamen Rider Club (Japan).nes", 70},
      {"Pocket Zaurus - Juu Ouken no Nazo (Japan).nes", 152},
      {"Saint Seiya - Ougon Densetsu (Japan).nes", 152},
      {"Space Shadow (Japan).nes", 70},
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    const auto expected = expected_roms.find(entry.path().filename().string());
    if (!entry.is_regular_file() || expected == expected_roms.end())
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    ASSERT_EQ(emulator_->GetRomData()->mapper, expected->second)
        << entry.path();
    if (expected->second == 70) {
      EXPECT_EQ(emulator_->GetRomData()->name_table_mirroring,
                NametableMirroring::kVertical)
          << entry.path();
    }

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
