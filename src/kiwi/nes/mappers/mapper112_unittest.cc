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

constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

void WriteRegister(Mapper* mapper, Byte target, Byte value) {
  mapper->WritePRG(0x8000, target);
  mapper->WritePRG(0xa000, value);
}

}  // namespace

class Mapper112Test : public MapperTest {};

TEST_F(Mapper112Test, MapsPRGAndLowerCHRWindows) {
  auto cartridge = LoadMapper(112, 16, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 0, 3);
  WriteRegister(mapper, 1, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(30 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));

  WriteRegister(mapper, 2, 0x20);
  WriteRegister(mapper, 3, 0x30);
  const size_t expected_banks[] = {0x20, 0x21, 0x30, 0x31};
  for (size_t window = 0; window < std::size(expected_banks); ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(expected_banks[window] * k1K + 0x123));
  }
}

TEST_F(Mapper112Test, AppliesIndependentOuterCHRBitsAndMirroring) {
  auto cartridge = LoadMapper(112, 16, 64, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Byte target = 4; target < 8; ++target)
    WriteRegister(mapper, target, target - 3);
  mapper->WritePRG(0xc000, 0xf0);

  for (size_t window = 4; window < 8; ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte((0x101 + window - 4) * k1K + 0x123));
  }

  mapper->WritePRG(0xe000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0xe001, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0xfffe, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper112Test, PersistsRegisterSelectionBanksAndCHRRAM) {
  auto cartridge = LoadMapper(112, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 0, 3);
  WriteRegister(mapper, 4, 3);
  mapper->WritePRG(0xe000, 1);
  mapper->WriteCHR(0x1123, 0x6d);
  mapper->WritePRG(0x8000, 4);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xa000, 4);
  mapper->WritePRG(0x8000, 0);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xe000, 0);
  mapper->WriteCHR(0x1123, 0x22);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x6d);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);

  mapper->WritePRG(0xa000, 5);
  mapper->WriteCHR(0x1123, 0xa5);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0xa5);
}

TEST_F(Mapper112Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, size_t> expected_roms = {
      {"Chik Bik Ji Jin - Saam Gwok Ji (Asia) (Unl).nes", 0},
      {"Cobra Mission (Asia) (Unl).nes", 0},
      {"Fighting Hero III (Asia) (Unl).nes", 0x2000},
      {"Huang Di (Asia) (Unl).nes", 0},
      {"Master Shooter (Asia) (Unl).nes", 0},
      {"San Guo Zhi - Qun Xiong Zheng Ba (Asia) (Unl).nes", 0},
      {"Zhen Ben Xi You Ji (Asia) (Unl).nes", 0},
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    const auto expected = expected_roms.find(entry.path().filename().string());
    if (!entry.is_regular_file() || expected == expected_roms.end()) {
      continue;
    }

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    ASSERT_EQ(emulator_->GetRomData()->mapper, 112) << entry.path();
    EXPECT_EQ(emulator_->GetRomData()->prg_ram_size, expected->second)
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
