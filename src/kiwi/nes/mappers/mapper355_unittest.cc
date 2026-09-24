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

class Mapper355Test : public MapperTest {};

TEST_F(Mapper355Test, CreatesThreeDBlockWithNROMLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(355));
  auto cartridge = LoadMapper(355, 2, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_FALSE(mapper->HasPRGRAM());
}

TEST_F(Mapper355Test, ProvidesWritableCharacterRAM) {
  auto cartridge = LoadMapper(355, 2, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0000, 0x35);
  mapper->WriteCHR(0x1fff, 0xa5);
  EXPECT_EQ(mapper->ReadCHR(0x0000), 0x35);
  EXPECT_EQ(mapper->ReadCHR(0x1fff), 0xa5);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1fff), 0x1fffu);
}

TEST_F(Mapper355Test, ArmsProtectionIRQOnlyAtFourEZeroZero) {
  auto cartridge = LoadMapper(355, 2, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());

  mapper->WriteExtendedRAM(0x4800, 0x32);
  mapper->WriteExtendedRAM(0x4900, 0x37);
  mapper->WriteExtendedRAM(0x4a00, 0x01);
  for (int cycle = 0; cycle < 64; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);

  mapper->WriteExtendedRAM(0x4e00, 0x18);
  for (int cycle = 0; cycle < 15; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper355Test, PersistsProtectionAndCharacterRAMState) {
  auto cartridge = LoadMapper(355, 2, 0, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WriteExtendedRAM(0x4e00, 0x18);
  for (int cycle = 0; cycle < 5; ++cycle)
    mapper->M2CycleIRQ();
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteCHR(0x0123, 0xa5);
  mapper->Reset();
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  for (int cycle = 0; cycle < 10; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper355Test, CorrectsAndRendersHwangShinweiROMFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "3-D Block (Asia) (Unl) (Hwang Shinwei).nes";
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
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->crc), 0x86dba660u);
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->mapper), 355u);

    emulator_->Run();
    Colors previous_pixels;
    bool rendered = false;
    bool frame_changed = false;
    size_t final_unique_colors = 0;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      final_unique_colors =
          std::set<Color>(pixels.begin(), pixels.end()).size();
      rendered = rendered || final_unique_colors > 1;
      if (!previous_pixels.empty() && pixels != previous_pixels)
        frame_changed = true;
      previous_pixels = pixels;
    }

    EXPECT_TRUE(rendered) << entry.path();
    EXPECT_TRUE(frame_changed) << entry.path();
    EXPECT_GT(final_unique_colors, 1u) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, 1u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
