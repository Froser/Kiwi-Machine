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
constexpr size_t k8K = 0x2000;

struct BankExpectation {
  Byte value;
  size_t prg_bank;
  size_t chr_bank;
};

}  // namespace

class Mapper086Test : public MapperTest {};

TEST_F(Mapper086Test, CreatesJalecoJF13WithResetLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(86));
  auto cartridge = LoadMapper(86, 8, 8, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_FALSE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xf456), TestPRGByte(0x7456));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper086Test, SelectsAllProgramAndCharacterBanksFromRegisterBits) {
  auto cartridge = LoadMapper(86, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const std::array<BankExpectation, 8> expectations{{
      {0x00, 0, 0},
      {0x11, 1, 1},
      {0x22, 2, 2},
      {0x33, 3, 3},
      {0x40, 0, 4},
      {0x51, 1, 5},
      {0x62, 2, 6},
      {0x73, 3, 7},
  }};

  for (const auto& expectation : expectations) {
    mapper->WriteExtendedRAM(0x6000, expectation.value);
    EXPECT_EQ(mapper->ReadPRG(0x8123),
              TestPRGByte(expectation.prg_bank * k32K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xf456),
              TestPRGByte(expectation.prg_bank * k32K + 0x7456));
    EXPECT_EQ(mapper->ReadCHR(0x0456),
              TestCHRByte(expectation.chr_bank * k8K + 0x0456));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456),
              expectation.chr_bank * k8K + 0x0456);
  }
}

TEST_F(Mapper086Test, DecodesBankRegistersAndHardwareMirrors) {
  auto cartridge = LoadMapper(86, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6000, 0x11);
  mapper->WriteExtendedRAM(0x5fff, 0x73);
  mapper->WriteExtendedRAM(0x7000, 0x73);
  mapper->WriteExtendedRAM(0x7fff, 0x73);
  mapper->WritePRG(0xdfff, 0x73);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(k8K + 0x0456));

  mapper->WritePRG(0xe000, 0x73);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(7 * k8K + 0x0456));

  mapper->WritePRG(0xefff, 0x40);
  mapper->WritePRG(0xf000, 0x00);
  mapper->WritePRG(0xffff, 0x73);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(4 * k8K + 0x0456));
}

TEST_F(Mapper086Test, RestoresBanksAndResetsWithoutWritableMemory) {
  auto cartridge = LoadMapper(86, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6fff, 0x73);
  const Byte original_chr = mapper->ReadCHR(0x0456);
  mapper->WriteCHR(0x0456, original_chr ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0456), original_chr);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6000), 0x60);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x6000, 0x00);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(7 * k8K + 0x0456));

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
}

TEST_F(Mapper086Test, RendersBothMoeroProYakyuuDumpsFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, uint32_t> expected_roms{
      {"Moero!! Pro Yakyuu (Black) (Japan).nes", 0x30bf2dbau},
      {"Moero!! Pro Yakyuu (Japan).nes", 0x5d2444d7u},
  };
  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file())
      continue;
    const auto expected = expected_roms.find(entry.path().filename().string());
    if (expected == expected_roms.end())
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    ASSERT_EQ(emulator_->GetRomData()->crc, expected->second) << entry.path();
    ASSERT_EQ(emulator_->GetRomData()->mapper, 86) << entry.path();

    emulator_->Run();
    Colors previous_pixels;
    bool frame_changed = false;
    bool final_frame_rendered = false;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      const std::set<Color> unique_colors(pixels.begin(), pixels.end());
      final_frame_rendered = unique_colors.size() > 1;
      if (!previous_pixels.empty() && pixels != previous_pixels)
        frame_changed = true;
      previous_pixels = pixels;
    }
    EXPECT_TRUE(final_frame_rendered) << entry.path();
    EXPECT_TRUE(frame_changed) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, expected_roms.size());
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
