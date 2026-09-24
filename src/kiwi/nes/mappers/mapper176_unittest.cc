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
#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k32K = 0x8000;
constexpr size_t k16K = 0x4000;
constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

void WriteMMC3Bank(Mapper* mapper, Byte target, Byte value, Byte mode = 0) {
  mapper->WritePRG(0x8000, static_cast<Byte>(target | mode));
  mapper->WritePRG(0x8001, value);
}

}  // namespace

class Mapper176Test : public MapperTest {};

TEST_F(Mapper176Test, CreatesFS005WithInitialMMC3Layout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(176));
  auto cartridge = LoadMapper(176, 64, 0, 0x02, 2, 0, 0x90);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(126 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(127 * k8K + 0x0123));
}

TEST_F(Mapper176Test, SelectsFS005ThirtyTwoAndSixteenKiBPRGBanks) {
  auto cartridge = LoadMapper(176, 64, 0, 0x02, 2, 0, 0x90);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5ff0, 0x24);
  mapper->WriteExtendedRAM(0x5ff1, 0x06);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(4 * k32K - 1));

  mapper->WriteExtendedRAM(0x5ff0, 0x23);
  mapper->WriteExtendedRAM(0x5ff1, 0x05);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(5 * k16K + 0x0123));
}

TEST_F(Mapper176Test, CombinesOuterPRGBaseWithMMC3Registers) {
  auto cartridge = LoadMapper(176, 128, 0, 0x02, 2, 0, 0x90);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5ff0, 0x20);
  mapper->WriteExtendedRAM(0x5ff1, 0x20);
  WriteMMC3Bank(mapper, 6, 3);
  WriteMMC3Bank(mapper, 7, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(67 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(68 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(126 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(127 * k8K + 0x0123));
}

TEST_F(Mapper176Test, BanksCHRRAMWithMMC3Registers) {
  auto cartridge = LoadMapper(176, 64, 0, 0x02, 2, 0, 0x90);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Bank(mapper, 2, 10);
  mapper->WriteCHR(0x1123, 0x5a);
  WriteMMC3Bank(mapper, 2, 11);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0);
  mapper->WriteCHR(0x1123, 0xa5);

  WriteMMC3Bank(mapper, 2, 10);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x5a);
  WriteMMC3Bank(mapper, 2, 11);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0xa5);
}

TEST_F(Mapper176Test, PrioritizesCNROMModeOverCHRRAMSelection) {
  auto cartridge = LoadMapper(176, 8, 8, 0, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5ff0, 0x60);
  mapper->WriteExtendedRAM(0x5ff2, 1);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(8 * k1K + 0x0123));
  mapper->WriteCHR(0x0123, 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(8 * k1K + 0x0123));

  mapper->WriteExtendedRAM(0x5ff0, 0x20);
  mapper->WriteCHR(0x0123, 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
}

TEST_F(Mapper176Test, SelectsMirroringAndWorkRAMBanks) {
  auto cartridge = LoadMapper(176, 64, 0, 0x02, 2, 0, 0x90);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_EQ(mapper->GetPRGNVRAMSize(), 4 * k8K);

  mapper->WritePRG(0xa000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0xa000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->WritePRG(0xa001, static_cast<Byte>(0x20 | bank));
    mapper->WriteExtendedRAM(0x6123, static_cast<Byte>(0x50 + bank));
  }
  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->WritePRG(0xa001, static_cast<Byte>(0x20 | bank));
    EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x50 + bank);
  }

  mapper->WritePRG(0xa001, 0x28);
  mapper->WritePRG(0xa000, 2);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  mapper->WritePRG(0xa000, 3);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
}

TEST_F(Mapper176Test, RunsMMC3IRQAndRestoresState) {
  auto cartridge = LoadMapper(176, 64, 0, 0x02, 2, 0, 0x90);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5ff0, 0x24);
  mapper->WriteExtendedRAM(0x5ff1, 0x06);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  mapper->PPUAddressChanged(0x0000);
  mapper->PPUAddressChanged(0x1000);
  EXPECT_EQ(irq_count_, 0);
  mapper->PPUAddressChanged(0x0000);
  mapper->PPUAddressChanged(0x1000);
  EXPECT_EQ(irq_count_, 1);
  mapper->WriteCHR(0x0123, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x5ff1, 0);
  mapper->WritePRG(0xa000, 0);
  mapper->WritePRG(0xe000, 0);
  mapper->WriteCHR(0x0123, 0xd6);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k32K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);
}

TEST_F(Mapper176Test, RendersAllFS005AcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, Byte> expected_roms = {
      {"Shu Qi Yu - Zhi Li Xiao Zhuan Yuan (China) (Unl).nes", 0},
      {"Xing He Zhan Shi (China) (Unl).nes", 2},
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
    EXPECT_EQ(emulator_->GetRomData()->mapper, 176) << entry.path();
    EXPECT_EQ(emulator_->GetRomData()->submapper, expected->second)
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
