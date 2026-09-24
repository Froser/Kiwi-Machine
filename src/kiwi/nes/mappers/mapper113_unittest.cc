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

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k32K = 0x8000;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper113Test : public MapperTest {};

TEST_F(Mapper113Test, MapsExpandedPRGAndCHRFields) {
  auto cartridge = LoadMapper(113, 16, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4100, 0x6b);

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(5 * k32K));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(6 * k32K - 1));
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(11 * k8K + 0x0123));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 11u * k8K + 0x0123);
}

TEST_F(Mapper113Test, ControlsMirroringAndDecodesRegisterAddresses) {
  auto cartridge = LoadMapper(113, 16, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WriteExtendedRAM(0x4100, 0xeb);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mirroring_changes_, 1);

  mapper->WriteExtendedRAM(0x4200, 0);
  mapper->WriteExtendedRAM(0x6000, 0);
  mapper->WritePRG(0x8000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(5 * k32K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(11 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mirroring_changes_, 1);
}

TEST_F(Mapper113Test, PersistsBankAndMirroringState) {
  auto cartridge = LoadMapper(113, 16, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5fff, 0xea);
  const Bytes state = SerializeMapper(mapper);
  mapper->WriteExtendedRAM(0x4100, 0x11);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(5 * k32K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(10 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper113Test, RendersAllAcceptanceROMs) {
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
    if (mapper_id != 113)
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

  EXPECT_EQ(verified_roms, 10u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
