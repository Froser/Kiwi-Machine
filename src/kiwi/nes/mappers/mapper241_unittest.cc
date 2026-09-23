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

constexpr size_t k32K = 0x8000;

}  // namespace

class Mapper241Test : public MapperTest {};

TEST_F(Mapper241Test, CreatesMapperWithInitialMemoryLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(241));
  auto cartridge = LoadMapper(241, 64, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(k32K - 1));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_TRUE(mapper->HasBatteryBackedRAM());
  EXPECT_EQ(mapper->GetPRGNVRAMSize(), 0x2000u);
}

TEST_F(Mapper241Test, SelectsThirtyTwoKiBProgramBankWithoutBusConflicts) {
  auto cartridge = LoadMapper(241, 64, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  ASSERT_EQ(mapper->ReadPRG(0x8000), 0);
  mapper->WritePRG(0x8000, 0x11);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(17 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(18 * k32K - 1));

  mapper->WritePRG(0xffff, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(31 * k32K));
}

TEST_F(Mapper241Test, PersistsCHRRAMWorkRAMAndMapperState) {
  auto cartridge = LoadMapper(241, 64, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x9000, 0x1d);
  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WriteExtendedRAM(0x6123, 0x6d);
  EXPECT_TRUE(mapper->IsPRGNVRAMDirty());
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x9000, 0x02);
  mapper->WriteCHR(0x0123, 0xa5);
  mapper->WriteExtendedRAM(0x6123, 0xd6);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(29 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x6d);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x6d);
}

TEST_F(Mapper241Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Fan Kong Jing Ying (China) (Unl).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 241) << entry.path();

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
