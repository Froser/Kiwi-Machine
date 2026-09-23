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

void WriteIRQLatch(Mapper* mapper, uint16_t value) {
  mapper->WritePRG(0x8000, value & 0x0f);
  mapper->WritePRG(0x9000, (value >> 4) & 0x0f);
  mapper->WritePRG(0xa000, (value >> 8) & 0x0f);
  mapper->WritePRG(0xb000, (value >> 12) & 0x0f);
}

}  // namespace

class Mapper073Test : public MapperTest {};

TEST_F(Mapper073Test, MapsProgramAndCharacterRAMWithFixedMirroring) {
  ASSERT_TRUE(Mapper::IsMapperSupported(73));
  auto cartridge = LoadMapper(73, 8, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  mapper->WritePRG(0xfabc, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
  mapper->WritePRG(0xffff, 0x0b);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WriteCHR(0x1fff, 0xa5);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0xa5);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1fff), 0x1fffu);

  ASSERT_TRUE(mapper->HasPRGRAM());
  mapper->WriteExtendedRAM(0x6123, 0x69);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x69);

  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xcfff, 0xff);
  mapper->WritePRG(0xdfff, 0xff);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mirroring_changes_, 0);
}

TEST_F(Mapper073Test, Counts16BitIRQAndUsesEnableAfterAcknowledge) {
  auto cartridge = LoadMapper(73, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());

  WriteIRQLatch(mapper, 0xfffe);
  mapper->WritePRG(0xc000, 0x03);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0xd000, 0);
  mapper->M2CycleIRQ();
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);

  mapper->WritePRG(0xc000, 0x02);
  mapper->M2CycleIRQ();
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 3);
  mapper->WritePRG(0xd000, 0);
  for (int cycle = 0; cycle < 4; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 3);
}

TEST_F(Mapper073Test, Counts8BitIRQAndPreservesCounterAcrossControlAndAck) {
  auto cartridge = LoadMapper(73, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8fff, 0xae);
  mapper->WritePRG(0x9abc, 0xbf);
  mapper->WritePRG(0xafff, 0xc2);
  mapper->WritePRG(0xbeef, 0xd1);
  mapper->WritePRG(0xc123, 0x07);

  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->WritePRG(0xcfff, 0x05);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);

  mapper->WritePRG(0xd999, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);
}

TEST_F(Mapper073Test, RestoresStateAndResetReturnsToPowerOnMapping) {
  auto cartridge = LoadMapper(73, 8, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xf000, 5);
  mapper->WriteCHR(0x0123, 0x5a);
  WriteIRQLatch(mapper, 0xfffe);
  mapper->WritePRG(0xc000, 0x03);
  mapper->M2CycleIRQ();
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xf000, 1);
  mapper->WriteCHR(0x0123, 0xa5);
  mapper->WritePRG(0xc000, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(5 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(7 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  for (int cycle = 0; cycle < 4; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper073Test, RendersSalamanderFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Salamander (Japan).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 73) << entry.path();

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
