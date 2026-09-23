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

class Mapper188Test : public MapperTest {};

TEST_F(Mapper188Test, MapsInternalAndExternalProgramROM) {
  ASSERT_TRUE(Mapper::IsMapperSupported(188));
  auto cartridge = LoadMapper(188, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  mapper->WritePRG(0xc04f, 0x02);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(10 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  mapper->WritePRG(0xc04f, 0x15);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));
}

TEST_F(Mapper188Test, AppliesBusConflictsAndUsesOpenBusWithoutExpansionROM) {
  auto combined = LoadMapper(188, 16, 0);
  ASSERT_TRUE(combined);
  Mapper* mapper = combined->mapper();

  ASSERT_EQ(mapper->ReadPRG(0x8000), 0);
  mapper->WritePRG(0x8000, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(8 * k16K + 0x0123));

  auto internal_only = LoadMapper(188, 8, 0);
  ASSERT_TRUE(internal_only);
  mapper = internal_only->mapper();
  mapper->WritePRG(0xc04f, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x81);

  mapper->WritePRG(0x8123, 0x13);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k16K + 0x0123));
}

TEST_F(Mapper188Test, ProvidesUnbankedCHRRAMAndIdleMicrophoneInput) {
  auto cartridge = LoadMapper(188, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WriteCHR(0x1fff, 0xa5);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0xa5);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1fff), 0x1fffu);

  EXPECT_FALSE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->GetExtendedRAMPointer(), nullptr);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x63);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7fff), 0x7b);
  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x63);
}

TEST_F(Mapper188Test, SelectsMirroringAndPersistsMapperState) {
  auto cartridge = LoadMapper(188, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WritePRG(0xc04f, 0x33);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k16K));
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xc04f, 0x14);
  mapper->WriteCHR(0x0123, 0xa5);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);

  mapper->WritePRG(0xc04f, 0x10);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mirroring_changes_, 3);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(7 * k16K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
}

TEST_F(Mapper188Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Karaoke Studio (Japan).nes",
      "Karaoke Studio Senyou Cassette Vol. 1 (Japan).nes",
      "Karaoke Studio Senyou Cassette Vol. 2 (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 188) << entry.path();

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
