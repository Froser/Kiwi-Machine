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

#include <array>
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

constexpr size_t k32K = 0x8000;

}  // namespace

class Mapper177Test : public MapperTest {};

TEST_F(Mapper177Test, CreatesHenggedianziBoardWithInitialLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(177));
  auto cartridge = LoadMapper(177, 64, 0, 0x03);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(k32K - 1));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper177Test, SelectsThirtyTwoKiBPRGBankWithoutBusConflicts) {
  auto cartridge = LoadMapper(177, 64, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  ASSERT_EQ(mapper->ReadPRG(0x8000), 0);
  mapper->WritePRG(0x8000, 0x11);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(17 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(18 * k32K - 1));

  mapper->WritePRG(0xffff, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(31 * k32K));
}

TEST_F(Mapper177Test, ControlsMirroringWithBitFive) {
  auto cartridge = LoadMapper(177, 64, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0x9000, 0x00);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mirroring_changes_, 1);

  mapper->WritePRG(0x9000, 0x20);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mirroring_changes_, 2);

  mapper->WritePRG(0x9000, 0x20);
  EXPECT_EQ(mirroring_changes_, 2);
}

TEST_F(Mapper177Test, SupportsCHRRAMAndBatteryBackedWorkRAM) {
  auto cartridge = LoadMapper(177, 64, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_TRUE(mapper->HasPRGRAM());
  EXPECT_TRUE(mapper->HasBatteryBackedRAM());
  EXPECT_EQ(mapper->GetPRGNVRAMSize(), 0x2000u);

  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WriteCHR(0x1fff, 0xa5);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0xa5);

  mapper->WriteExtendedRAM(0x6123, 0x3c);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x3c);
  EXPECT_TRUE(mapper->IsPRGNVRAMDirty());
}

TEST_F(Mapper177Test, SelectsThirtyTwoKiBWorkRAMBanks) {
  auto cartridge = LoadMapper(177, 64, 0, 0x02, 1, 0, 0x90);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_EQ(mapper->GetPRGNVRAMSize(), 4u * 0x2000);

  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->WritePRG(0x8000, static_cast<Byte>(bank << 6));
    mapper->WriteExtendedRAM(0x6123, static_cast<Byte>(0x40 + bank));
  }
  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->WritePRG(0x8000, static_cast<Byte>(bank << 6));
    EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x40 + bank);
  }
}

TEST_F(Mapper177Test, SwitchesSubmapperOneCHRRAMFromNametableAddress) {
  auto cartridge = LoadMapper(177, 64, 0, 0, 1, 0, 0x09);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_TRUE(mapper->UsesCustomPPUMemoryMapping());
  std::array<Byte, 0x800> ciram{};

  mapper->WritePRG(0x8000, 0x20);
  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->ReadPPUMemoryByte(ciram.data(), 0x2000 + bank * 0x100);
    mapper->WriteCHR(0x0123, static_cast<Byte>(0x50 + bank));
  }
  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->ReadPPUMemoryByte(ciram.data(), 0x2000 + bank * 0x100);
    EXPECT_EQ(mapper->ReadCHR(0x0123), 0x50 + bank);
  }
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x53);
}

TEST_F(Mapper177Test, ResetsAndRestoresMapperState) {
  auto cartridge = LoadMapper(177, 64, 0, 0x03);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x35);
  mapper->WriteCHR(0x0456, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 0x02);
  mapper->WriteCHR(0x0456, 0xd6);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(21 * k32K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0x6d);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0x6d);
}

TEST_F(Mapper177Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Mei Guo Fu Hao - American Man (China) (Unl).nes",
      "Shang Gu Shen Jian (China) (Unl).nes",
      "Wang Zi Fu Chou Ji (China) (Unl).nes",
      "Xing Ji Zheng Ba (China) (Unl).nes",
      "Xing Zhan Qing Yuan (China) (Unl).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 177) << entry.path();

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
