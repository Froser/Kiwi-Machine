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
#include "nes/ppu_bus.h"

namespace kiwi {
namespace nes {
namespace testing {

class Mapper118Test : public MapperTest {};

TEST_F(Mapper118Test, CreatesTxSROMMapperAndKeepsMMC3Banking) {
  ASSERT_TRUE(Mapper::IsMapperSupported(118));
  auto cartridge = LoadMapper(118, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 3);
  WriteMMC3Register(mapper, 7, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * 0x2000 + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * 0x2000 + 0x0123));

  WriteMMC3Register(mapper, 0, 0x82);
  WriteMMC3Register(mapper, 2, 0x87);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x82 * 0x0400 + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x1123), TestCHRByte(0x87 * 0x0400 + 0x0123));
}

TEST_F(Mapper118Test, RoutesNormalModeNametablesFromTwoKilobyteCHRRegisters) {
  auto cartridge = LoadMapper(118, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  ASSERT_TRUE(mapper->UsesCustomPPUMemoryMapping());
  WriteMMC3Register(mapper, 0, 0x80);
  WriteMMC3Register(mapper, 1, 0x00);

  ppu_bus.Write(0x2001, 0x5a);
  ppu_bus.Write(0x2801, 0xa5);
  EXPECT_EQ(ppu_bus.Read(0x2401), 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0xa5);
  EXPECT_EQ(ppu_bus.Read(0x3001), 0x5a);
}

TEST_F(Mapper118Test, RoutesInvertedModeNametablesFromOneKilobyteCHRRegisters) {
  auto cartridge = LoadMapper(118, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  WriteMMC3Register(mapper, 2, 0x80, 0x80);
  WriteMMC3Register(mapper, 3, 0x00, 0x80);
  WriteMMC3Register(mapper, 4, 0x80, 0x80);
  WriteMMC3Register(mapper, 5, 0x00, 0x80);

  ppu_bus.Write(0x2001, 0x3c);
  ppu_bus.Write(0x2401, 0xc3);
  EXPECT_EQ(ppu_bus.Read(0x2801), 0x3c);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0xc3);
}

TEST_F(Mapper118Test, IgnoresMMC3MirroringRegister) {
  auto cartridge = LoadMapper(118, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const NametableMirroring initial_mirroring = mapper->GetNametableMirroring();
  mapper->WritePRG(0xa000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), initial_mirroring);
  EXPECT_EQ(mirroring_changes_, 0);
}

TEST_F(Mapper118Test, PersistsNametablePagesWithMMC3State) {
  auto cartridge = LoadMapper(118, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  WriteMMC3Register(mapper, 0, 0x80);
  WriteMMC3Register(mapper, 1, 0x00);
  WriteMMC3Register(mapper, 6, 3);
  ppu_bus.Write(0x2001, 0x5a);
  ppu_bus.Write(0x2802, 0xa5);
  const Bytes state = SerializeMapper(mapper);

  WriteMMC3Register(mapper, 0, 0x00);
  WriteMMC3Register(mapper, 1, 0x80);
  WriteMMC3Register(mapper, 6, 1);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * 0x2000));
  EXPECT_EQ(ppu_bus.Read(0x2401), 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x2c02), 0xa5);
}

TEST_F(Mapper118Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Alien Syndrome (USA) (Unl).nes",
      "Armadillo (Japan).nes",
      "Eric Cantona Football Challenge - Goal! 2 (Europe).nes",
      "Goal! Two (USA).nes",
      "NES Play Action Football (USA).nes",
      "Pro Sport Hockey (USA).nes",
      "Ys III - Wanderers From Ys (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 118) << entry.path();

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
