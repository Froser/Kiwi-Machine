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
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper047Test : public MapperTest {};

TEST_F(Mapper047Test, SelectsBlocksOnlyWhenWorkRAMIsWritable) {
  ASSERT_TRUE(Mapper::IsMapperSupported(47));
  auto cartridge = LoadMapper(47, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_FALSE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->GetExtendedRAMPointer(), nullptr);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(14 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(15 * k8K + 0x0123));

  mapper->WriteExtendedRAM(0x6000, 1);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));

  mapper->WritePRG(0xa001, 0x80);
  mapper->WriteExtendedRAM(0x7fff, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(16 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(30 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0155), TestCHRByte(128 * k1K + 0x0155));

  mapper->WritePRG(0xa001, 0xc0);
  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(16 * k8K + 0x0123));

  mapper->WritePRG(0xa001, 0);
  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(16 * k8K + 0x0123));
}

TEST_F(Mapper047Test, ConstrainsInnerBanksToTheSelectedBlock) {
  auto cartridge = LoadMapper(47, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa001, 0x80);
  mapper->WriteExtendedRAM(0x6000, 1);
  WriteMMC3Register(mapper, 6, 0x32);
  WriteMMC3Register(mapper, 7, 0x23);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(18 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(19 * k8K + 0x0123));

  WriteMMC3Register(mapper, 6, 0x3d, 0x40);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(30 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(29 * k8K + 0x0123));

  WriteMMC3Register(mapper, 2, 0xff, 0x80);
  EXPECT_EQ(mapper->ReadCHR(0x0155), TestCHRByte(255 * k1K + 0x0155));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0155), 255u * k1K + 0x0155);

  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadCHR(0x0155), TestCHRByte(127 * k1K + 0x0155));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0155), 127u * k1K + 0x0155);
}

TEST_F(Mapper047Test, PersistsBlockProtectionAndMMC3State) {
  auto cartridge = LoadMapper(47, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa001, 0x80);
  mapper->WriteExtendedRAM(0x6000, 1);
  WriteMMC3Register(mapper, 6, 2);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  mapper->WritePRG(0xa001, 0xc0);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xa001, 0x80);
  mapper->WriteExtendedRAM(0x6000, 0);
  WriteMMC3Register(mapper, 6, 3);
  mapper->WritePRG(0xa000, 0);
  mapper->WritePRG(0xe000, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(18 * k8K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(18 * k8K + 0x0123));
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);

  mapper->Reset();
  mapper->WriteExtendedRAM(0x6000, 1);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k8K + 0x0123));
}

TEST_F(Mapper047Test, RendersSuperSpikeVBallAndNintendoWorldCup) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom =
      "Super Spike V'Ball + Nintendo World Cup (USA).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 47) << entry.path();

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
