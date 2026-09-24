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

constexpr size_t k16K = 0x4000;

}  // namespace

class Mapper232Test : public MapperTest {};

TEST_F(Mapper232Test, CreatesBF9096WithInitialBankLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(232));
  auto cartridge = LoadMapper(232, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k16K + 0x0123));
}

TEST_F(Mapper232Test, CombinesOuterBlockAndInnerPage) {
  auto cartridge = LoadMapper(232, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x10);
  mapper->WritePRG(0xc000, 1);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(9 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(11 * k16K + 0x0123));

  mapper->WritePRG(0xbfff, 0x08);
  mapper->WritePRG(0xffff, 2);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(6 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));
}

TEST_F(Mapper232Test, SwapsOuterBlockBitsForSubmapperOne) {
  auto cartridge = LoadMapper(232, 16, 0, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x10);
  mapper->WritePRG(0xc000, 2);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(6 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  mapper->WritePRG(0x8000, 0x08);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(10 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(11 * k16K + 0x0123));
}

TEST_F(Mapper232Test, SupportsCHRRAM) {
  auto cartridge = LoadMapper(232, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0000, 0x12);
  mapper->WriteCHR(0x1fff, 0x34);
  EXPECT_EQ(mapper->ReadCHR(0x0000), 0x12);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x34);
}

TEST_F(Mapper232Test, PersistsBanksAndCHRRAM) {
  auto cartridge = LoadMapper(232, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x10);
  mapper->WritePRG(0xc000, 1);
  mapper->WriteCHR(0x0123, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 0);
  mapper->WritePRG(0xc000, 0);
  mapper->WriteCHR(0x0123, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(9 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(11 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
}

TEST_F(Mapper232Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Pegasus 4 in 1 (Unl).nes",
      "Quattro Adventure (USA) (Unl).nes",
      "Quattro Arcade (USA) (Unl).nes",
      "Quattro Sports (USA) (Unl).nes",
      "Super Sports Challenge (Europe) (Unl) (Plug-Thru Cart).nes",
      "Super Sports Challenge (Europe) (Unl).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 232) << entry.path();

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
