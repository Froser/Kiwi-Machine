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

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

}  // namespace

class Mapper065Test : public MapperTest {};

TEST_F(Mapper065Test, MapsInitialAndSwappablePRGWindows) {
  ASSERT_TRUE(Mapper::IsMapperSupported(65));
  auto cartridge = LoadMapper(65, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(1 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(30 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));

  mapper->WritePRG(0x8ff0, 3);
  mapper->WritePRG(0xaff7, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(30 * k8K + 0x0123));

  mapper->WritePRG(0x9ff0, 0x80);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(30 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k8K + 0x0123));

  mapper->WritePRG(0xc000, 7);
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k8K + 0x0123));
}

TEST_F(Mapper065Test, MapsEightIndependentOneKiBCHRWindows) {
  auto cartridge = LoadMapper(65, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr std::array<Byte, 8> kBanks{{3, 5, 7, 9, 11, 13, 15, 17}};
  for (size_t window = 0; window < kBanks.size(); ++window) {
    const Address register_address = static_cast<Address>(0xb000 + window);
    mapper->WritePRG(register_address, kBanks[window]);
  }

  for (size_t window = 0; window < kBanks.size(); ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    const size_t expected = kBanks[window] * k1K + 0x123;
    EXPECT_EQ(mapper->ReadCHR(address), TestCHRByte(expected));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address), expected);
  }
}

TEST_F(Mapper065Test, SelectsAllHardwareMirroringModes) {
  auto cartridge = LoadMapper(65, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr std::array<Byte, 4> kValues{{0x00, 0x40, 0x80, 0xc0}};
  constexpr std::array<NametableMirroring, 4> kExpected{{
      NametableMirroring::kVertical,
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kHorizontal,
      NametableMirroring::kOneScreenLower,
  }};
  for (size_t index = 0; index < kValues.size(); ++index) {
    mapper->WritePRG(0x9001, kValues[index]);
    EXPECT_EQ(mapper->GetNametableMirroring(), kExpected[index]);
  }
  EXPECT_EQ(mirroring_changes_, 4);
}

TEST_F(Mapper065Test, CountsIRQOnM2CyclesAndStopsAtZero) {
  auto cartridge = LoadMapper(65, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());

  mapper->WritePRG(0x9005, 0x01);
  mapper->WritePRG(0x9006, 0x02);
  mapper->WritePRG(0x9004, 0);
  mapper->WritePRG(0x9003, 0x80);
  for (int cycle = 0; cycle < 257; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  for (int cycle = 0; cycle < 512; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0x9005, 0);
  mapper->WritePRG(0x9006, 2);
  mapper->WritePRG(0x9004, 0);
  mapper->WritePRG(0x9003, 0x80);
  mapper->M2CycleIRQ();
  mapper->WritePRG(0x9003, 0);
  for (int cycle = 0; cycle < 8; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper065Test, PersistsRegistersIRQAndCHRRAMThenResetsRegisters) {
  auto cartridge = LoadMapper(65, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0xa000, 4);
  mapper->WritePRG(0xb002, 5);
  mapper->WritePRG(0x9000, 0x80);
  mapper->WritePRG(0x9001, 0x40);
  mapper->WriteCHR(0x0923, 0x5a);
  mapper->WritePRG(0x9005, 0);
  mapper->WritePRG(0x9006, 3);
  mapper->WritePRG(0x9004, 0);
  mapper->WritePRG(0x9003, 0x80);
  mapper->M2CycleIRQ();
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0xb002, 0);
  mapper->WritePRG(0x9000, 0);
  mapper->WritePRG(0x9001, 0x80);
  mapper->WritePRG(0x9003, 0);
  mapper->WriteCHR(0x0923, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(30 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x0923), 0x5a);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(30 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  for (int cycle = 0; cycle < 8; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper065Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Daiku no Gen San 2 - Akage no Dan no Gyakushuu (Japan).nes",
      "Kaiketsu Yanchamaru 3 - Taiketsu! Zouringen (Japan).nes",
      "Spartan X 2 (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 65) << entry.path();

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
