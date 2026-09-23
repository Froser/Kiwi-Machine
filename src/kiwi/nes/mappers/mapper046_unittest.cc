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
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper046Test : public MapperTest {};

TEST_F(Mapper046Test, CombinesOuterAndInnerProgramAndCharacterBanks) {
  ASSERT_TRUE(Mapper::IsMapperSupported(46));
  auto cartridge = LoadMapper(46, 64, 128, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(k32K - 1));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));

  mapper->WriteExtendedRAM(0x6000, 0xa3);
  mapper->WritePRG(0x8000, 0x71);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(7 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(8 * k32K - 1));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(87 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 87u * k8K + 0x0456);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->WriteExtendedRAM(0x5fff, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(7 * k32K + 0x0123));
}

TEST_F(Mapper046Test, PersistsRegistersAndCharacterRAMThenResets) {
  auto cartridge = LoadMapper(46, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x7fff, 0x02);
  mapper->WritePRG(0xffff, 0x01);
  mapper->WriteCHR(0x0456, 0xa5);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x6000, 0);
  mapper->WritePRG(0x8000, 0);
  mapper->WriteCHR(0x0456, 0x5a);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa5);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa5);
}

TEST_F(Mapper046Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Nintendo Test Cart (USA).nes",
      "Rumble Station - 15 in 1 (USA) (Unl).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 46) << entry.path();

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
