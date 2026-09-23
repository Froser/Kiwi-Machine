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
#include "nes/ppu_bus.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k32K = 0x8000;
constexpr size_t k2K = 0x0800;

void WriteRegister(Mapper* mapper, Byte target, Byte value) {
  mapper->WriteExtendedRAM(0x4100, target);
  mapper->WriteExtendedRAM(0x4101, value);
}

void WriteNametables(PPUBus* ppu_bus) {
  ppu_bus->Write(0x2000, 0x10);
  ppu_bus->Write(0x2400, 0x20);
  ppu_bus->Write(0x2800, 0x30);
  ppu_bus->Write(0x2c00, 0x40);
}

}  // namespace

class Mapper139Test : public MapperTest {};

TEST_F(Mapper139Test, MapsProgramAndRewiredCharacterBanks) {
  ASSERT_TRUE(Mapper::IsMapperSupported(139));
  auto cartridge = LoadMapper(139, 8, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5f00, 0xfd);
  mapper->WriteExtendedRAM(0x5f01, 0xfb);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  mapper->WriteExtendedRAM(0x4200, 5);
  mapper->WriteExtendedRAM(0x4201, 1);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));

  WriteRegister(mapper, 0, 6);
  WriteRegister(mapper, 1, 3);
  WriteRegister(mapper, 2, 2);
  WriteRegister(mapper, 3, 1);
  WriteRegister(mapper, 4, 5);

  constexpr std::array<size_t, 4> kExpectedBanks{{184, 173, 170, 167}};
  for (size_t window = 0; window < kExpectedBanks.size(); ++window) {
    const Address address = static_cast<Address>(window * k2K + 0x0123);
    const size_t expected = kExpectedBanks[window] * k2K + 0x0123;
    EXPECT_EQ(mapper->ReadCHR(address), TestCHRByte(expected));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address), expected);
  }
}

TEST_F(Mapper139Test, AppliesSimpleModeWithoutDroppingPpuAddressBits) {
  auto cartridge = LoadMapper(139, 2, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 0, 2);
  WriteRegister(mapper, 1, 3);
  WriteRegister(mapper, 2, 4);
  WriteRegister(mapper, 3, 5);
  WriteRegister(mapper, 4, 1);
  WriteRegister(mapper, 7, 1);

  constexpr std::array<size_t, 4> kExpectedBanks{{40, 41, 42, 43}};
  for (size_t window = 0; window < kExpectedBanks.size(); ++window) {
    const Address address = static_cast<Address>(window * k2K + 0x0056);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(kExpectedBanks[window] * k2K + 0x0056));
  }
}

TEST_F(Mapper139Test, RoutesAllNametableModesIncludingAsymmetricMode) {
  auto cartridge = LoadMapper(139, 2, 8, 0x09);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);
  ASSERT_TRUE(mapper->UsesCustomPPUMemoryMapping());

  WriteNametables(&ppu_bus);
  EXPECT_EQ(ppu_bus.Read(0x2000), 0x30);
  EXPECT_EQ(ppu_bus.Read(0x2400), 0x40);
  EXPECT_EQ(ppu_bus.Read(0x2800), 0x30);
  EXPECT_EQ(ppu_bus.Read(0x2c00), 0x40);

  WriteRegister(mapper, 7, 0x02);
  WriteNametables(&ppu_bus);
  EXPECT_EQ(ppu_bus.Read(0x2000), 0x20);
  EXPECT_EQ(ppu_bus.Read(0x2400), 0x20);
  EXPECT_EQ(ppu_bus.Read(0x2800), 0x40);
  EXPECT_EQ(ppu_bus.Read(0x2c00), 0x40);

  WriteRegister(mapper, 7, 0x04);
  WriteNametables(&ppu_bus);
  EXPECT_EQ(ppu_bus.Read(0x2000), 0x10);
  EXPECT_EQ(ppu_bus.Read(0x2400), 0x40);
  EXPECT_EQ(ppu_bus.Read(0x2800), 0x40);
  EXPECT_EQ(ppu_bus.Read(0x2c00), 0x40);

  WriteRegister(mapper, 7, 0x06);
  WriteNametables(&ppu_bus);
  EXPECT_EQ(ppu_bus.Read(0x2000), 0x40);
  EXPECT_EQ(ppu_bus.Read(0x2400), 0x40);
  EXPECT_EQ(ppu_bus.Read(0x2800), 0x40);
  EXPECT_EQ(ppu_bus.Read(0x2c00), 0x40);

  WriteRegister(mapper, 7, 0x07);
  WriteNametables(&ppu_bus);
  EXPECT_EQ(ppu_bus.Read(0x2000), 0x30);
  EXPECT_EQ(ppu_bus.Read(0x2400), 0x40);
  EXPECT_EQ(ppu_bus.Read(0x2800), 0x30);
  EXPECT_EQ(ppu_bus.Read(0x2c00), 0x40);
  EXPECT_EQ(mirroring_changes_, 4);
}

TEST_F(Mapper139Test, RestoresRegistersAndCurrentIndexThenResets) {
  auto cartridge = LoadMapper(139, 8, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 5, 3);
  WriteRegister(mapper, 3, 2);
  WriteRegister(mapper, 4, 5);
  WriteRegister(mapper, 7, 4);
  mapper->WriteExtendedRAM(0x4100, 3);
  const Bytes state = SerializeMapper(mapper);

  WriteRegister(mapper, 5, 1);
  WriteRegister(mapper, 3, 0);
  WriteRegister(mapper, 7, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k32K));
  EXPECT_EQ(mapper->ReadCHR(0x1923), TestCHRByte(171 * k2K + 0x0123));

  mapper->WriteExtendedRAM(0x4101, 4);
  EXPECT_EQ(mapper->ReadCHR(0x1923), TestCHRByte(179 * k2K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k32K));

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper139Test, RendersMahjangCompanionFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom =
      "Mahjang Companion (Asia) (Unl) (Sachen).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 139) << entry.path();

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
