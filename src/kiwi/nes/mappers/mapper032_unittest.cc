// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <array>
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

constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

struct ExpectedROM {
  Byte submapper;
  size_t prg_ram_size;
};

}  // namespace

class Mapper032Test : public MapperTest {};

TEST_F(Mapper032Test, MapsPRGBanksInBothModesAndControlsMirroring) {
  auto cartridge = LoadMapper(32, 16, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8123, 3);
  mapper->WritePRG(0xafff, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(30 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));

  mapper->WritePRG(0x9abc, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(30 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);

  mapper->WritePRG(0x9000, 2);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper032Test, DecodesAllMirroredCHRBankRegisters) {
  auto cartridge = LoadMapper(32, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr std::array<Address, 8> kRegisterAddresses = {
      0xb000, 0xb101, 0xb202, 0xb303, 0xb404, 0xb505, 0xb606, 0xbff7,
  };
  for (size_t window = 0; window < kRegisterAddresses.size(); ++window)
    mapper->WritePRG(kRegisterAddresses[window], 0x20 + window);

  for (size_t window = 0; window < kRegisterAddresses.size(); ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte((0x20 + window) * k1K + 0x123));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address),
              (0x20 + window) * k1K + 0x123);
  }
}

TEST_F(Mapper032Test, KeepsMajorLeagueModeAndMirroringHardwired) {
  auto cartridge = LoadMapper(32, 16, 16, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0xa000, 4);
  mapper->WritePRG(0x9000, 3);

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(30 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
  EXPECT_EQ(mirroring_changes_, 0);
}

TEST_F(Mapper032Test, PersistsBanksModeMirroringAndCHRRAM) {
  auto cartridge = LoadMapper(32, 8, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0xa000, 4);
  mapper->WritePRG(0x9000, 3);
  mapper->WriteCHR(0x0123, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0xa000, 2);
  mapper->WritePRG(0x9000, 0);
  mapper->WriteCHR(0x0123, 0x22);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper032Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, ExpectedROM> expected_roms = {
      {"Ai Sensei no Oshiete - Watashi no Hoshi (Japan).nes", {0, 0}},
      {"Image Fight (Japan).nes", {0, 0x2000}},
      {"Kaiketsu Yanchamaru 2 - Karakuri Land (Japan).nes", {0, 0}},
      {"Major League (Japan).nes", {1, 0}},
      {"Meikyuu Jima (Japan).nes", {0, 0}},
      {"Perman - Enban wo Torikaese!! (Japan).nes", {0, 0}},
      {"Perman Part 2 - Himitsu Kessha Madoodan wo Taose! (Japan).nes", {0, 0}},
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
    EXPECT_EQ(emulator_->GetRomData()->mapper, 32) << entry.path();
    EXPECT_EQ(emulator_->GetRomData()->submapper, expected->second.submapper)
        << entry.path();
    EXPECT_EQ(emulator_->GetRomData()->prg_ram_size,
              expected->second.prg_ram_size)
        << entry.path();

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
