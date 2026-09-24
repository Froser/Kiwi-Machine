// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

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

constexpr size_t k32K = 0x8000;
constexpr size_t k8K = 0x2000;

void WriteRegister(Mapper* mapper, Byte target, Byte value) {
  mapper->WriteExtendedRAM(0x4100, target);
  mapper->WriteExtendedRAM(0x4101, value);
}

}  // namespace

class Mapper150Test : public MapperTest {};

TEST_F(Mapper150Test, MapsMapper150PRGAndCHRRegisterBits) {
  auto cartridge = LoadMapper(150, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 5, 3);
  WriteRegister(mapper, 4, 1);
  WriteRegister(mapper, 6, 2);

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(6 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 6u * k8K + 0x0456);
}

TEST_F(Mapper150Test, MapsMapper243RewiredCHRRegisterBits) {
  auto cartridge = LoadMapper(243, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 2, 1);
  WriteRegister(mapper, 4, 1);
  WriteRegister(mapper, 6, 2);

  EXPECT_EQ(mapper->ReadCHR(0x0321), TestCHRByte(11 * k8K + 0x0321));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0321), 11u * k8K + 0x0321);
}

TEST_F(Mapper150Test, DecodesMirroredIndexAndReadableDataRegisters) {
  auto cartridge = LoadMapper(150, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5f00, 5);
  mapper->WriteExtendedRAM(0x5f01, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k32K));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5f01), 0x5b);

  mapper->WriteExtendedRAM(0x4200, 5);
  mapper->WriteExtendedRAM(0x4201, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k32K));
}

TEST_F(Mapper150Test, RoutesAllFourNametableModes) {
  auto cartridge = LoadMapper(150, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  ppu_bus.Write(0x2001, 0x11);
  ppu_bus.Write(0x2c01, 0x44);
  EXPECT_EQ(ppu_bus.Read(0x2401), 0x11);
  EXPECT_EQ(ppu_bus.Read(0x2801), 0x11);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0x44);

  WriteRegister(mapper, 7, 0x02);
  ppu_bus.Write(0x2002, 0x12);
  ppu_bus.Write(0x2802, 0x34);
  EXPECT_EQ(ppu_bus.Read(0x2402), 0x12);
  EXPECT_EQ(ppu_bus.Read(0x2c02), 0x34);

  WriteRegister(mapper, 7, 0x04);
  ppu_bus.Write(0x2003, 0x56);
  ppu_bus.Write(0x2403, 0x78);
  EXPECT_EQ(ppu_bus.Read(0x2803), 0x56);
  EXPECT_EQ(ppu_bus.Read(0x2c03), 0x78);

  WriteRegister(mapper, 7, 0x06);
  ppu_bus.Write(0x2c04, 0x9a);
  EXPECT_EQ(ppu_bus.Read(0x2004), 0x9a);
  EXPECT_EQ(ppu_bus.Read(0x2404), 0x9a);
  EXPECT_EQ(ppu_bus.Read(0x2804), 0x9a);
}

TEST_F(Mapper150Test, PersistsRegistersMirroringAndCHRRAM) {
  auto cartridge = LoadMapper(150, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 5, 3);
  WriteRegister(mapper, 7, 0x04);
  mapper->WriteCHR(0x0123, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  WriteRegister(mapper, 5, 0);
  WriteRegister(mapper, 7, 0);
  mapper->WriteCHR(0x0123, 0x22);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k32K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper150Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "2 in 1 Lightgun Game - Cosmocop + Cyber Monster (Asia) (Unl).nes",
      "2 in 1 Lightgun Game - Tough Cop + Super Tough Cop (Asia) (Unl).nes",
      "Auto-Upturn (Asia) (Unl) (NES).nes",
      "Chess Academy (Asia) (Unl) (Famicom).nes",
      "Chinese Checkers (Asia) (Unl) (NES).nes",
      "Happy Pairs (Asia) (Unl) (NES).nes",
      "Honey Peach - Mei Nv Quan (Asia) (Unl).nes",
      "Magic Cube (Asia) (Unl) (NES).nes",
      "Mahjong Academy (Asia) (Unl).nes",
      "Olympic I.Q. (Asia) (Unl) (NES).nes",
      "Poker II (Asia) (Unl).nes",
      "Poker III 5 in 1 (Asia) (Unl).nes",
      "Strategist (Asia) (Unl) (Famicom).nes",
      "Strategist (Asia) (Unl) (NES).nes",
      "Tasac (Asia) (Unl).nes",
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    const std::string filename = entry.path().filename().string();
    if (!entry.is_regular_file() || !expected_roms.contains(filename))
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    const Byte expected_mapper =
        filename == "Honey Peach - Mei Nv Quan (Asia) (Unl).nes" ? 243 : 150;
    ASSERT_EQ(emulator_->GetRomData()->mapper, expected_mapper) << entry.path();

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
