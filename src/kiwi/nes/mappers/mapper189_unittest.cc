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
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k1K = 0x0400;
constexpr size_t k32K = 0x8000;

}  // namespace

class Mapper189Test : public MapperTest {};

TEST_F(Mapper189Test, Selects32KProgramBanksFromEitherDataNibble) {
  ASSERT_TRUE(Mapper::IsMapperSupported(189));
  auto cartridge = LoadMapper(189, 16, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_FALSE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(0x6123));

  mapper->WriteExtendedRAM(0x411f, 0x40);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));

  mapper->WriteExtendedRAM(0x4120, 0x40);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(4 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(4 * k32K + 0x6123));

  mapper->WriteExtendedRAM(0x5fff, 0x04);
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k32K + 0x2123));

  mapper->WriteExtendedRAM(0x6000, 0x21);
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k32K + 0x4123));

  mapper->WriteExtendedRAM(0x7fff, 0x70);
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(7 * k32K + 0x6123));
}

TEST_F(Mapper189Test, KeepsProgramBankFixedAcrossMMC3RegisterWrites) {
  auto cartridge = LoadMapper(189, 16, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4132, 0x52);
  WriteMMC3Register(mapper, 6, 1);
  WriteMMC3Register(mapper, 7, 2);
  WriteMMC3Register(mapper, 6, 3, 0x40);

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(7 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(7 * k32K + 0x2123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k32K + 0x4123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(7 * k32K + 0x6123));
}

TEST_F(Mapper189Test, RetainsMMC3CharacterMirroringAndIRQBehavior) {
  auto cartridge = LoadMapper(189, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 0, 0x22);
  WriteMMC3Register(mapper, 2, 0x25);
  EXPECT_EQ(mapper->ReadCHR(0x0155), TestCHRByte(0x22 * k1K + 0x0155));
  EXPECT_EQ(mapper->ReadCHR(0x0555), TestCHRByte(0x23 * k1K + 0x0155));
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(0x25 * k1K + 0x0155));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1155), 0x25u * k1K + 0x0155);

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

TEST_F(Mapper189Test, PersistsOuterBankAndMMC3StateThenResetsOuterBank) {
  auto cartridge = LoadMapper(189, 16, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4132, 0x50);
  WriteMMC3Register(mapper, 2, 9);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x4132, 0x20);
  WriteMMC3Register(mapper, 2, 3);
  mapper->WritePRG(0xa000, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(9 * k1K + 0x0155));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(9 * k1K + 0x0155));
}

TEST_F(Mapper189Test, RendersThunderWarriorFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Thunder Warrior (Asia) (Unl).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->crc, 0x3eea372eu) << entry.path();
    ASSERT_EQ(emulator_->GetRomData()->mapper, 189) << entry.path();

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

  EXPECT_EQ(verified_roms, 1u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
