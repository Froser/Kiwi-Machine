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

constexpr size_t k4K = 0x1000;

}  // namespace

class Mapper184Test : public MapperTest {};

TEST_F(Mapper184Test, CreatesSunsoft1MapperWithFixedPRG) {
  ASSERT_TRUE(Mapper::IsMapperSupported(184));
  auto cartridge = LoadMapper(184, 2, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));
  mapper->WritePRG(0x8000, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
}

TEST_F(Mapper184Test, SelectsTwoFourKilobyteCHRWindows) {
  auto cartridge = LoadMapper(184, 2, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6000, 0x23);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(3 * k4K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x1123), TestCHRByte(6 * k4K + 0x0123));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 3 * k4K + 0x0123);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1123), 6 * k4K + 0x0123);

  mapper->WriteExtendedRAM(0x7fff, 0x07);
  EXPECT_EQ(mapper->ReadCHR(0x0000), TestCHRByte(7 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(4 * k4K));

  const Byte original = mapper->ReadCHR(0x0456);
  mapper->WriteCHR(0x0456, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0456), original);
}

TEST_F(Mapper184Test, RegisterPrecludesPRGRAMAndPersistsState) {
  auto cartridge = LoadMapper(184, 2, 4, 0, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6001, 0x12);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(5 * k4K));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x60);

  const Bytes state = SerializeMapper(mapper);
  mapper->WriteExtendedRAM(0x6000, 0x03);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k4K));
  EXPECT_EQ(mapper->ReadCHR(0x1000), TestCHRByte(5 * k4K));
}

TEST_F(Mapper184Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Atlantis no Nazo (Japan) (Beta).nes",
      "Atlantis no Nazo (Japan).nes",
      "Kanshakudama Nage Kantarou no Toukaidou Gojuusan Tsugi (Japan).nes",
      "Wing of Madoola, The (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 184) << entry.path();

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
