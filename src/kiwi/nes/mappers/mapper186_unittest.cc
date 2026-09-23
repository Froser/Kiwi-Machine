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
#include "nes/ppu_bus.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k16K = 0x4000;

}  // namespace

class Mapper186Test : public MapperTest {};

TEST_F(Mapper186Test, CreatesStudyBoxWithInitialMemoryLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(186));
  auto cartridge = LoadMapper(186, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kFourScreen);
  EXPECT_TRUE(mapper->HasPRGRAM());
  EXPECT_FALSE(mapper->HasBatteryBackedRAM());
  EXPECT_EQ(cartridge->GetRomData()->prg_ram_size, 0x10000u);
  EXPECT_EQ(mapper->GetExtendedRAMPointer(), nullptr);
}

TEST_F(Mapper186Test, SelectsLowerProgramBankAndKeepsBIOSBankFixed) {
  auto cartridge = LoadMapper(186, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4201, 11);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(11 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(0x0123));

  mapper->WriteExtendedRAM(0x4201, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(15 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(0x0123));
}

TEST_F(Mapper186Test, MapsAllStudyBoxWorkRAMWindows) {
  auto cartridge = LoadMapper(186, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4401, 0x48);
  mapper->WriteExtendedRAM(0x5002, 0x58);
  mapper->WriteExtendedRAM(0x6001, 0x60);
  mapper->WriteExtendedRAM(0x7001, 0x70);

  mapper->WriteExtendedRAM(0x4200, 0xc5);
  mapper->WriteExtendedRAM(0x5001, 0x5d);
  mapper->WriteExtendedRAM(0x6001, 0x66);
  mapper->WriteExtendedRAM(0x7001, 0x77);

  EXPECT_EQ(mapper->ReadExtendedRAM(0x4401), 0x48);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5001), 0x5d);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x66);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7001), 0x77);

  mapper->WriteExtendedRAM(0x4200, 0x00);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5002), 0x58);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x60);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7001), 0x70);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4300), 0x43);
}

TEST_F(Mapper186Test, ProvidesCharacterRAMAndFourIndependentNametables) {
  auto cartridge = LoadMapper(186, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  ppu_bus.Write(0x0123, 0x51);
  ppu_bus.Write(0x1fff, 0x5f);
  EXPECT_EQ(ppu_bus.Read(0x0123), 0x51);
  EXPECT_EQ(ppu_bus.Read(0x1fff), 0x5f);

  ppu_bus.Write(0x2001, 0x20);
  ppu_bus.Write(0x2401, 0x24);
  ppu_bus.Write(0x2801, 0x28);
  ppu_bus.Write(0x2c01, 0x2c);
  EXPECT_EQ(ppu_bus.Read(0x2001), 0x20);
  EXPECT_EQ(ppu_bus.Read(0x2401), 0x24);
  EXPECT_EQ(ppu_bus.Read(0x2801), 0x28);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0x2c);
}

TEST_F(Mapper186Test, ExposesIdleTapeRegistersAndReadyDelay) {
  auto cartridge = LoadMapper(186, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());

  EXPECT_EQ(mapper->ReadExtendedRAM(0x4200), 0xaa);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4201), 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4202), 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4203), 0);

  mapper->WriteExtendedRAM(0x4202, 0x11);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4201), 0x80);
  for (int cycle = 0; cycle < 99; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4202), 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4202), 0x40);

  mapper->WriteExtendedRAM(0x4202, 0x20);
  mapper->WriteExtendedRAM(0x4202, 0x00);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4202), 0x40);
  EXPECT_EQ(irq_count_, 0);
}

TEST_F(Mapper186Test, PersistsBanksRAMAndRegisterStateThenResets) {
  auto cartridge = LoadMapper(186, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4200, 0xc5);
  mapper->WriteExtendedRAM(0x4201, 11);
  mapper->WriteExtendedRAM(0x4202, 0x11);
  mapper->WriteExtendedRAM(0x5001, 0x5d);
  mapper->WriteExtendedRAM(0x6001, 0x66);
  mapper->WriteCHR(0x0123, 0x5a);
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);
  ppu_bus.Write(0x2c01, 0x2c);
  for (int cycle = 0; cycle < 40; ++cycle)
    mapper->M2CycleIRQ();
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x4200, 0);
  mapper->WriteExtendedRAM(0x4201, 1);
  mapper->WriteExtendedRAM(0x4202, 0);
  mapper->WriteCHR(0x0123, 0xa5);
  ppu_bus.Write(0x2c01, 0xc2);
  ASSERT_TRUE(DeserializeMapper(mapper, state));

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(11 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5001), 0x5d);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x66);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0x2c);
  for (int cycle = 0; cycle < 60; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4202), 0x40);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5001), 0);
}

TEST_F(Mapper186Test, RendersStudyBoxBIOSFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Study Box (Japan).nes";
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
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->crc), 0x0d473ee6u);
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->mapper), 186u);

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
