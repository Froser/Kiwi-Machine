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

constexpr size_t kINESHeaderSize = 0x10;
constexpr size_t k16K = 0x4000;
constexpr size_t k4K = 0x1000;
constexpr size_t k32K = 0x8000;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k16K + (address & (k16K - 1));
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper168Test : public MapperTest {};

TEST_F(Mapper168Test, CreatesRacerMateWithInitialMemoryLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(168));
  auto cartridge = LoadMapper(168, 4, 8, 0x03);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k16K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_FALSE(mapper->HasPRGRAM());
  EXPECT_TRUE(mapper->HasBatteryBackedRAM());
  EXPECT_EQ(mapper->GetPRGNVRAMSize(), k32K);
}

TEST_F(Mapper168Test, SelectsProgramAndUpperCharacterBanks) {
  auto cartridge = LoadMapper(168, 4, 8, 0x03);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xf080, 0xff);
  mapper->WritePRG(0xf000, 0x00);
  mapper->WritePRG(0x8000, 0x8b);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x1123), TestCHRByte(11 * k4K + 0x0123));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 0x0123u);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1123), 11u * k4K + 0x0123);
}

TEST_F(Mapper168Test, SelectsBanksWithoutProgramROMBusConflicts) {
  Bytes rom = MakeTestROM(168, 4, 8, 0x03);
  SetPRGByte(&rom, 0, 0x8123, 0);

  auto cartridge = base::MakeRefCounted<Cartridge>(
      static_cast<EmulatorImpl*>(emulator_.get()));
  ASSERT_TRUE(cartridge->Load(rom).success);
  cartridge->mapper()->Reset();
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8123, 0x82);
  EXPECT_EQ(mapper->ReadPRG(0x8124), TestPRGByte(2 * k16K + 0x0124));
  EXPECT_EQ(mapper->ReadCHR(0x1124), TestCHRByte(2 * k4K + 0x0124));
}

TEST_F(Mapper168Test, ProtectsBatteryCharacterRAMUntilUnlockSequence) {
  auto cartridge = LoadMapper(168, 4, 8, 0x03);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x07);
  mapper->WriteCHR(0x1123, 0x57);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x57);

  mapper->WritePRG(0x8000, 0x08);
  mapper->WriteCHR(0x1123, 0xa5);
  mapper->WritePRG(0xf080, 0xff);
  mapper->WritePRG(0xf000, 0x00);
  EXPECT_EQ(mapper->ReadCHR(0x1123), TestCHRByte(8 * k4K + 0x0123));

  mapper->WriteCHR(0x1123, 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x5a);
}

TEST_F(Mapper168Test, RaisesPeriodicIRQAndHonorsAcknowledgeSequence) {
  auto cartridge = LoadMapper(168, 4, 8, 0x03);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());

  mapper->WritePRG(0xf080, 0xff);
  for (int cycle = 0; cycle < 32; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);

  mapper->WritePRG(0xf000, 0x00);
  for (int cycle = 0; cycle < 1023; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  for (int cycle = 0; cycle < 2047; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);

  mapper->WritePRG(0xf080, 0xff);
  for (int cycle = 0; cycle < 2048; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);
  mapper->WritePRG(0xf000, 0x00);
  for (int cycle = 0; cycle < 1024; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 3);
}

TEST_F(Mapper168Test, PersistsOnlyBatteryCharacterRAMAndRestoresState) {
  auto cartridge = LoadMapper(168, 4, 8, 0x03);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const auto initial_snapshot = mapper->ExportPRGNVRAM(false);
  ASSERT_TRUE(initial_snapshot);
  ASSERT_EQ(initial_snapshot->data.size(), k32K);
  EXPECT_EQ(initial_snapshot->data[0x0123], TestCHRByte(k32K + 0x0123));
  EXPECT_FALSE(mapper->ExportPRGNVRAM(true));

  mapper->WritePRG(0xf080, 0xff);
  mapper->WritePRG(0xf000, 0x00);
  mapper->WritePRG(0x8000, 0x08);
  mapper->WriteCHR(0x1123, 0x5a);
  mapper->WritePRG(0x8000, 0x07);
  mapper->WriteCHR(0x1123, 0x67);

  const auto dirty_snapshot = mapper->ExportPRGNVRAM(true);
  ASSERT_TRUE(dirty_snapshot);
  ASSERT_EQ(dirty_snapshot->data.size(), k32K);
  EXPECT_EQ(dirty_snapshot->data[0x0123], 0x5a);
  EXPECT_NE(dirty_snapshot->data[0x7123], 0x67);

  const Bytes state = SerializeMapper(mapper);
  mapper->WritePRG(0x8000, 0x08);
  mapper->WriteCHR(0x1123, 0xa5);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x67);

  Bytes replacement(k32K);
  replacement[0x0123] = 0x3c;
  EXPECT_TRUE(mapper->ImportPRGNVRAM(replacement));
  mapper->WritePRG(0x8000, 0x08);
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x3c);
  EXPECT_FALSE(mapper->IsPRGNVRAMDirty());
  EXPECT_FALSE(mapper->ImportPRGNVRAM(Bytes(1)));
}

TEST_F(Mapper168Test, RendersRacerMateChallengeIIFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom =
      "Racermate Challenge II (USA) (Unl) (v6.02.002).nes";
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
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->crc), 0x95191b8bu);
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->mapper), 168u);

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
