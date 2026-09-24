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
namespace {

constexpr size_t k16K = 0x4000;
constexpr size_t k2K = 0x0800;
constexpr size_t k1K = 0x0400;

}  // namespace

class Mapper068Test : public MapperTest {};

TEST_F(Mapper068Test, CreatesSunsoft4MapperAndMapsPRG) {
  ASSERT_TRUE(Mapper::IsMapperSupported(68));
  auto cartridge = LoadMapper(68, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  mapper->WritePRG(0xf000, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xbfff), TestPRGByte(4 * k16K - 1));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(8 * k16K - 1));

  mapper->WritePRG(0xf000, 0x0f);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(7 * k16K));
}

TEST_F(Mapper068Test, MapsFourTwoKilobyteCHRWindows) {
  auto cartridge = LoadMapper(68, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Byte window = 0; window < 4; ++window)
    mapper->WritePRG(0x8000 + window * 0x1000, 0x10 + window);

  for (size_t window = 0; window < 4; ++window) {
    const Address address = static_cast<Address>(window * k2K + 0x0123);
    const size_t expected = (0x10 + window) * k2K + 0x0123;
    EXPECT_EQ(mapper->ReadCHR(address), TestCHRByte(expected));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address), expected);
  }

  const Byte original = mapper->ReadCHR(0x0123);
  mapper->WriteCHR(0x0123, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0123), original);
}

TEST_F(Mapper068Test, RoutesCIRAMWithAllFourMirroringModes) {
  auto cartridge = LoadMapper(68, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  ASSERT_TRUE(mapper->UsesCustomPPUMemoryMapping());
  constexpr NametableMirroring kExpectedMirroring[] = {
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
  };
  for (Byte mode = 0; mode < std::size(kExpectedMirroring); ++mode) {
    mapper->WritePRG(0xe000, mode);
    EXPECT_EQ(mapper->GetNametableMirroring(), kExpectedMirroring[mode]);
  }

  mapper->WritePRG(0xe000, 0);
  ppu_bus.Write(0x2001, 0x5a);
  ppu_bus.Write(0x2401, 0xa5);
  EXPECT_EQ(ppu_bus.Read(0x2801), 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0xa5);

  mapper->WritePRG(0xe000, 1);
  ppu_bus.Write(0x2002, 0x3c);
  ppu_bus.Write(0x2802, 0xc3);
  EXPECT_EQ(ppu_bus.Read(0x2402), 0x3c);
  EXPECT_EQ(ppu_bus.Read(0x2c02), 0xc3);
}

TEST_F(Mapper068Test, MapsReadOnlyCHRROMIntoNametableWindows) {
  auto cartridge = LoadMapper(68, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xd000, 2);
  mapper->WritePRG(0xe000, 0x10);

  constexpr Address kOffset = 0x0123;
  EXPECT_EQ(ppu_bus.Read(0x2000 + kOffset), TestCHRByte(0x81 * k1K + kOffset));
  EXPECT_EQ(ppu_bus.Read(0x2400 + kOffset), TestCHRByte(0x82 * k1K + kOffset));
  EXPECT_EQ(ppu_bus.Read(0x2800 + kOffset), TestCHRByte(0x81 * k1K + kOffset));
  EXPECT_EQ(ppu_bus.Read(0x2c00 + kOffset), TestCHRByte(0x82 * k1K + kOffset));

  const Byte original = ppu_bus.Read(0x2000 + kOffset);
  ppu_bus.Write(0x2000 + kOffset, original ^ 0xff);
  EXPECT_EQ(ppu_bus.Read(0x2000 + kOffset), original);

  mapper->WritePRG(0xe000, 0x13);
  EXPECT_EQ(ppu_bus.Read(0x2000 + kOffset), TestCHRByte(0x82 * k1K + kOffset));
  EXPECT_EQ(ppu_bus.Read(0x2c00 + kOffset), TestCHRByte(0x82 * k1K + kOffset));
}

TEST_F(Mapper068Test, GatesWorkRAMWithPRGBankRegister) {
  auto cartridge = LoadMapper(68, 8, 32, 0, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);

  mapper->WritePRG(0xf000, 0x10);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);

  mapper->WritePRG(0xf000, 0);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
  mapper->WritePRG(0xf000, 0x10);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
}

TEST_F(Mapper068Test, TimesExternalOptionROMAccess) {
  auto cartridge = LoadMapper(68, 10, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());
  mapper->WritePRG(0xf000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x81);

  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(8 * k16K + 0x0123));
  for (int cycle = 0; cycle < 107519; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(8 * k16K + 0x0123));
  mapper->M2CycleIRQ();
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x81);

  mapper->WritePRG(0xf000, 1);
  mapper->WriteExtendedRAM(0x6000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(9 * k16K + 0x0123));

  mapper->WritePRG(0xf000, 0x08);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
}

TEST_F(Mapper068Test, PersistsBankingNametableModeAndWorkRAM) {
  auto cartridge = LoadMapper(68, 8, 32, 0, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0xc000, 4);
  mapper->WritePRG(0xd000, 5);
  mapper->WritePRG(0xe000, 0x11);
  mapper->WritePRG(0xf000, 0x12);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xd000, 2);
  mapper->WritePRG(0xe000, 0);
  mapper->WritePRG(0xf000, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(2 * k16K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(3 * k2K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(ppu_bus.Read(0x2001), TestCHRByte(0x84 * k1K + 1));
  EXPECT_EQ(ppu_bus.Read(0x2801), TestCHRByte(0x85 * k1K + 1));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
}

TEST_F(Mapper068Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "After Burner (USA) (Unl).nes",
      "After Burner II (Japan).nes",
      "Maharaja (Japan).nes",
      "Nantettatte!! Baseball (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 68) << entry.path();

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
