// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k32K = 0x8000;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper079Test : public MapperTest {};

TEST_F(Mapper079Test, DecodesRegistersAndMapsPRGAndCHR) {
  auto cartridge = LoadMapper(79, 4, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4100, 0x0e);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(k32K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(2 * k32K - 1));
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(6 * k8K + 0x0123));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 6u * k8K + 0x0123);

  mapper->WriteExtendedRAM(0x5fff, 0x03);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(3 * k8K + 0x0123));
}

TEST_F(Mapper079Test, IgnoresWritesOutsideDecodedRegisterAddresses) {
  auto cartridge = LoadMapper(79, 4, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4100, 0x0d);
  mapper->WriteExtendedRAM(0x4200, 0);
  mapper->WriteExtendedRAM(0x6000, 0);
  mapper->WritePRG(0x8000, 0);

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(k32K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(5 * k8K));
}

TEST_F(Mapper079Test, MirrorsSmallROMAndPersistsCHRRAM) {
  auto cartridge = LoadMapper(79, 1, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4100, 0x0f);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(0));

  mapper->WriteCHR(0x0123, 0x5a);
  const Bytes state = SerializeMapper(mapper);
  mapper->WriteCHR(0x0123, 0xa5);
  mapper->WriteExtendedRAM(0x4100, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
}

TEST_F(Mapper079Test, CorrectsMislabeledPipe5HeaderByPayloadCRC) {
  Bytes rom = MakeTestROM(142, 4, 2);
  ASSERT_GE(rom.size(), 4u);
  rom[rom.size() - 4] = 0xd6;
  rom[rom.size() - 3] = 0xcb;
  rom[rom.size() - 2] = 0x30;
  rom[rom.size() - 1] = 0x66;

  auto cartridge = base::MakeRefCounted<Cartridge>(
      static_cast<EmulatorImpl*>(emulator_.get()));
  ASSERT_TRUE(cartridge->Load(rom).success);
  ASSERT_TRUE(cartridge->GetRomData());
  EXPECT_EQ(static_cast<uint32_t>(cartridge->GetRomData()->crc), 0x58152b42u);
  EXPECT_EQ(cartridge->GetRomData()->mapper, 79);
}

TEST_F(Mapper079Test, RendersMislabeledPipe5For600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Pipe 5 (Asia) (Unl).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 79) << entry.path();

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

TEST_F(Mapper079Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nes")
      continue;

    std::array<Byte, 16> header{};
    std::ifstream stream(entry.path(), std::ios::binary);
    stream.read(reinterpret_cast<char*>(header.data()), header.size());
    if (stream.gcount() != header.size() || header[0] != 'N' ||
        header[1] != 'E' || header[2] != 'S' || header[3] != 0x1a) {
      continue;
    }

    const Byte mapper_id =
        static_cast<Byte>((header[6] >> 4) | (header[7] & 0xf0));
    if (mapper_id != 79)
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();

    emulator_->Run();
    bool rendered = false;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      const std::set<Color> unique_colors(pixels.begin(), pixels.end());
      if (unique_colors.size() > 1) {
        rendered = true;
        break;
      }
    }
    EXPECT_TRUE(rendered) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, 25u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
