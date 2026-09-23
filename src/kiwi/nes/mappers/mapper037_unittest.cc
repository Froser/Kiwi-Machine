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
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k1K = 0x0400;
constexpr size_t k8K = 0x2000;

struct OuterBankExpectation {
  Byte value;
  std::array<size_t, 4> prg_banks;
  size_t chr_bank;
};

}  // namespace

class Mapper037Test : public MapperTest {};

TEST_F(Mapper037Test, MapsEveryOuterProgramAndCharacterBlock) {
  ASSERT_TRUE(Mapper::IsMapperSupported(37));
  auto cartridge = LoadMapper(37, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_FALSE(mapper->HasPRGRAM());
  mapper->WritePRG(0xa001, 0x80);
  WriteMMC3Register(mapper, 6, 0x1d);
  WriteMMC3Register(mapper, 7, 0x16);
  WriteMMC3Register(mapper, 2, 0xff);

  const std::array<OuterBankExpectation, 8> expectations{{
      {0, {5, 6, 6, 7}, 127},
      {1, {5, 6, 6, 7}, 127},
      {2, {5, 6, 6, 7}, 127},
      {3, {13, 14, 14, 15}, 127},
      {4, {29, 22, 30, 31}, 255},
      {5, {29, 22, 30, 31}, 255},
      {6, {29, 22, 30, 31}, 255},
      {7, {29, 30, 30, 31}, 255},
  }};

  for (const auto& expectation : expectations) {
    mapper->WriteExtendedRAM(0x6000, expectation.value);
    EXPECT_EQ(mapper->ReadPRG(0x8123),
              TestPRGByte(expectation.prg_banks[0] * k8K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xa123),
              TestPRGByte(expectation.prg_banks[1] * k8K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xc123),
              TestPRGByte(expectation.prg_banks[2] * k8K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xe123),
              TestPRGByte(expectation.prg_banks[3] * k8K + 0x0123));
    EXPECT_EQ(mapper->ReadCHR(0x1155),
              TestCHRByte(expectation.chr_bank * k1K + 0x0155));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1155),
              expectation.chr_bank * k1K + 0x0155);
  }
}

TEST_F(Mapper037Test, AppliesOuterMaskInInvertedProgramMode) {
  auto cartridge = LoadMapper(37, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa001, 0x80);
  mapper->WriteExtendedRAM(0x6000, 3);
  WriteMMC3Register(mapper, 7, 2);
  WriteMMC3Register(mapper, 6, 5, 0x40);

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(14 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(10 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(13 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(15 * k8K + 0x0123));
}

TEST_F(Mapper037Test, SelectsOuterBlockOnlyWhenWorkRAMIsWritable) {
  auto cartridge = LoadMapper(37, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6000, 7);
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(7 * k8K + 0x0123));

  mapper->WritePRG(0xa001, 0x80);
  mapper->WriteExtendedRAM(0x7fff, 7);
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));

  mapper->WritePRG(0xa001, 0xc0);
  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));

  mapper->WritePRG(0xa001, 0);
  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));
}

TEST_F(Mapper037Test, RetainsMMC3MirroringAndIRQBehavior) {
  auto cartridge = LoadMapper(37, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xa000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);

  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper037Test, PersistsOuterBlockAndMMC3StateThenResetsBlock) {
  auto cartridge = LoadMapper(37, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa001, 0x80);
  mapper->WriteExtendedRAM(0x6000, 7);
  WriteMMC3Register(mapper, 2, 9);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x6000, 0);
  WriteMMC3Register(mapper, 2, 3);
  mapper->WritePRG(0xa000, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(137 * k1K + 0x0155));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(7 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(9 * k1K + 0x0155));
}

TEST_F(Mapper037Test, RendersBothSuperMarioTetrisWorldCupDumpsFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms{
      "Super Mario Bros. + Tetris + Nintendo World Cup (Europe).nes",
      "Super Mario Bros. + Tetris + Nintendo World Cup (Europe) (Rev A).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->crc, 0xf46ef39au) << entry.path();
    ASSERT_EQ(emulator_->GetRomData()->mapper, 37) << entry.path();

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
