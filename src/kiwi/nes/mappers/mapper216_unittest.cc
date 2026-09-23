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

constexpr size_t k32K = 0x8000;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper216Test : public MapperTest {};

TEST_F(Mapper216Test, StartsWithBankZeroAndHeaderMirroring) {
  ASSERT_TRUE(Mapper::IsMapperSupported(216));
  auto cartridge = LoadMapper(216, 4, 8, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(k32K - 1));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper216Test, SelectsProgramAndCharacterBanksFromWriteAddress) {
  auto cartridge = LoadMapper(216, 4, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x800f, 0x00);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(7 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 7u * k8K + 0x0456);

  mapper->WritePRG(0xfff2, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(k8K + 0x0456));
}

TEST_F(Mapper216Test, ExposesZeroAtBoardReadPort) {
  auto cartridge = LoadMapper(216, 4, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8001, 0xff);
  mapper->WriteExtendedRAM(0x5000, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5000), 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5001), 0x50);
}

TEST_F(Mapper216Test, PersistsBanksAndCharacterRAMThenResetsBanks) {
  auto cartridge = LoadMapper(216, 4, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8001, 0);
  mapper->WriteCHR(0x0123, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 0xff);
  mapper->WriteCHR(0x0123, 0xa5);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
}

TEST_F(Mapper216Test, RendersMagicJewelry2For600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Magic Jewelry 2 (Asia) (Unl).nes";
  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() ||
        entry.path().filename().string() != expected_rom) {
      continue;
    }

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    ASSERT_EQ(emulator_->GetRomData()->mapper, 216) << entry.path();

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

  EXPECT_EQ(verified_roms, 1u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
