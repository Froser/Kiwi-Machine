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

constexpr size_t k16K = 0x4000;
constexpr size_t k2K = 0x0800;

}  // namespace

class Mapper067Test : public MapperTest {};

TEST_F(Mapper067Test, MapsProgramAndFourCharacterWindows) {
  ASSERT_TRUE(Mapper::IsMapperSupported(67));
  auto cartridge = LoadMapper(67, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));
  mapper->WritePRG(0xf800, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  constexpr std::array<Address, 4> kRegisters{{
      0x8800,
      0x9800,
      0xa800,
      0xb800,
  }};
  constexpr std::array<Byte, 4> kBanks{{3, 5, 7, 9}};
  for (size_t window = 0; window < kRegisters.size(); ++window)
    mapper->WritePRG(kRegisters[window], kBanks[window]);

  for (size_t window = 0; window < kBanks.size(); ++window) {
    const Address address = static_cast<Address>(window * k2K + 0x0123);
    const size_t expected = kBanks[window] * k2K + 0x0123;
    EXPECT_EQ(mapper->ReadCHR(address), TestCHRByte(expected));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address), expected);
  }

  mapper->WritePRG(0x8000, 11);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(3 * k2K + 0x0123));
}

TEST_F(Mapper067Test, SelectsMirroringAndCountsDownOneShotIRQ) {
  auto cartridge = LoadMapper(67, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr std::array<NametableMirroring, 4> kExpected{{
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
  }};
  for (Byte value = 0; value < kExpected.size(); ++value) {
    mapper->WritePRG(0xe800, value);
    EXPECT_EQ(mapper->GetNametableMirroring(), kExpected[value]);
  }
  EXPECT_EQ(mirroring_changes_, 4);

  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());
  mapper->WritePRG(0xc800, 0);
  mapper->WritePRG(0xcfff, 2);
  mapper->WritePRG(0xd800, 0x10);
  mapper->M2CycleIRQ();
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  for (int cycle = 0; cycle < 8; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0xc800, 0xff);
  mapper->WritePRG(0xd800, 0);
  mapper->WritePRG(0xc800, 0);
  mapper->WritePRG(0xc800, 1);
  mapper->WritePRG(0xd800, 0x10);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);
}

TEST_F(Mapper067Test, PersistsBanksIRQMirroringAndCHRRAMThenResets) {
  auto cartridge = LoadMapper(67, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xf800, 3);
  mapper->WritePRG(0x9800, 2);
  mapper->WritePRG(0xe800, 2);
  mapper->WritePRG(0xc800, 0);
  mapper->WritePRG(0xc800, 1);
  mapper->WritePRG(0xd800, 0x10);
  mapper->M2CycleIRQ();
  mapper->WriteCHR(0x0923, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xf800, 1);
  mapper->WritePRG(0x9800, 0);
  mapper->WritePRG(0xe800, 0);
  mapper->WritePRG(0xd800, 0);
  mapper->WriteCHR(0x0923, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x0923), 0x5a);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(7 * k16K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  for (int cycle = 0; cycle < 8; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper067Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Fantasy Zone 2 - Opa-Opa no Namida (Japan).nes",
      "Mito Koumon - Sekai Manyuu Ki (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 67) << entry.path();

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
