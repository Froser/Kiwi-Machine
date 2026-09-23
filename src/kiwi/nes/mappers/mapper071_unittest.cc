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
#include <fstream>
#include <set>

#include "base/files/file_path.h"
#include "base/functional/bind.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k16K = 0x4000;

}  // namespace

class Mapper071Test : public MapperTest {};

TEST_F(Mapper071Test, MapsFullWidthSelectedBankAndFixedLastBank) {
  auto cartridge = LoadMapper(71, 40, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc000, 0x21);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0x21 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(0x21 * k16K + k16K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(39 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(40 * k16K - 1));
}

TEST_F(Mapper071Test, AutoDetectsBF9097Mirroring) {
  auto cartridge = LoadMapper(71, 8, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 2);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * k16K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->WritePRG(0x9000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
  mapper->WritePRG(0x8000, 0x10);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  EXPECT_EQ(mirroring_changes_, 2);

  mapper->WritePRG(0xc000, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(5 * k16K));
}

TEST_F(Mapper071Test, HonorsBF9097SubmapperFromReset) {
  auto cartridge = LoadMapper(71, 8, 0, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x10);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);

  mapper->Reset();
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0x8000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
}

TEST_F(Mapper071Test, PersistsBankMirroringAndCHRRAM) {
  auto cartridge = LoadMapper(71, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x9000, 0x10);
  mapper->WritePRG(0xc000, 4);
  mapper->WriteCHR(0x1fff, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x9000, 0);
  mapper->WritePRG(0xc000, 1);
  mapper->WriteCHR(0x1fff, 0xa5);
  ASSERT_TRUE(DeserializeMapper(mapper, state));

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(4 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0x5a);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
}

TEST_F(Mapper071Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nes")
      continue;

    std::array<Byte, 16> header{};
    std::ifstream stream(entry.path(), std::ios::binary);
    stream.read(reinterpret_cast<char*>(header.data()), header.size());
    if (stream.gcount() != header.size() || header[0] != 'N' ||
        header[1] != 'E' || header[2] != 'S' || header[3] != 0x1a) {
      continue;
    }

    const Byte mapper_id =
        static_cast<Byte>((header[6] >> 4) | (header[7] & 0xf0));
    if (mapper_id != 71)
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();

    emulator_->Run();
    bool rendered = false;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      const std::set<Color> unique_colors(pixels.begin(), pixels.end());

      if (unique_colors.size() > 1) {
        rendered = true;
        break;
      }
    }
    EXPECT_TRUE(rendered) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, 16u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
