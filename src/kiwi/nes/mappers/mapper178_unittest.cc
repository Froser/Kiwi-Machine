// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

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

constexpr size_t k16K = 0x4000;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper178Test : public MapperTest {};

TEST_F(Mapper178Test, CreatesWaixingBoardWithResetLayoutAndMemory) {
  ASSERT_TRUE(Mapper::IsMapperSupported(178));
  auto cartridge = LoadMapper(178, 64, 0, 0x03);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(k16K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_TRUE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->GetPRGNVRAMSize(), 4u * k8K);

  mapper->WriteCHR(0x0456, 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0x5a);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
}

TEST_F(Mapper178Test, SelectsEveryProgramModeAndMirroring) {
  auto cartridge = LoadMapper(178, 64, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4802, 0x02);
  mapper->WriteExtendedRAM(0x4801, 0x03);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(19 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(20 * k16K + 0x0123));

  mapper->WriteExtendedRAM(0x4ffc, 0x05);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(19 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(19 * k16K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mirroring_changes_, 1);

  mapper->WriteExtendedRAM(0x4800, 0x02);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(19 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(23 * k16K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->WriteExtendedRAM(0x4801, 0x02);
  mapper->WriteExtendedRAM(0x4800, 0x06);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(18 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(22 * k16K + 0x0123));
  EXPECT_EQ(mirroring_changes_, 2);
}

TEST_F(Mapper178Test, SelectsFourWorkRAMBanks) {
  auto cartridge = LoadMapper(178, 32, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->WriteExtendedRAM(0x4803, bank);
    mapper->WriteExtendedRAM(0x6123, static_cast<Byte>(0x40 + bank));
  }
  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->WriteExtendedRAM(0x4fff, bank);
    EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), static_cast<Byte>(0x40 + bank));
  }
}

TEST_F(Mapper178Test, ResetsRegistersAndRestoresCompleteState) {
  auto cartridge = LoadMapper(178, 64, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4802, 0x03);
  mapper->WriteExtendedRAM(0x4801, 0x05);
  mapper->WriteExtendedRAM(0x4800, 0x03);
  mapper->WriteExtendedRAM(0x4803, 0x02);
  mapper->WriteExtendedRAM(0x6000, 0x5a);
  mapper->WriteCHR(0x0123, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x4800, 0x00);
  mapper->WriteExtendedRAM(0x4801, 0x00);
  mapper->WriteExtendedRAM(0x4802, 0x00);
  mapper->WriteExtendedRAM(0x4803, 0x00);
  mapper->WriteExtendedRAM(0x6000, 0xa5);
  mapper->WriteCHR(0x0123, 0xb6);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(29 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(31 * k16K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(k16K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);
}

TEST_F(Mapper178Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "San Guo Zhong Lie Zhuan (China) (Unl).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 178) << entry.path();

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
