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

constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

void WriteRegister(Mapper* mapper, Byte command, Byte value) {
  mapper->WritePRG(0x8000, command);
  mapper->WritePRG(0xa000, value);
}

}  // namespace

class Mapper069Test : public MapperTest {};

TEST_F(Mapper069Test, MapsAllPRGAndCHRWindows) {
  auto cartridge = LoadMapper(69, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 0x09, 3);
  WriteRegister(mapper, 0x0a, 4);
  WriteRegister(mapper, 0x0b, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(5 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(31 * k8K));

  for (Byte window = 0; window < 8; ++window)
    WriteRegister(mapper, window, static_cast<Byte>(0x10 + window));
  for (size_t window = 0; window < 8; ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte((0x10 + window) * k1K + 0x123));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address),
              (0x10 + window) * k1K + 0x123);
  }
}

TEST_F(Mapper069Test, MapsBankedROMAndRAMAtSixThousand) {
  auto cartridge = LoadMapper(69, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 0x08, 3);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), TestPRGByte(3 * k8K + 0x0123));

  WriteRegister(mapper, 0x08, 0xc1);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  WriteRegister(mapper, 0x08, 0xc2);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
  WriteRegister(mapper, 0x08, 0xc1);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  WriteRegister(mapper, 0x08, 0xc2);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0xa5);

  WriteRegister(mapper, 0x08, 0x41);
  mapper->WriteExtendedRAM(0x6123, 0xff);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
  WriteRegister(mapper, 0x08, 0xc1);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
}

TEST_F(Mapper069Test, ControlsMirroringAndCountsDownIRQ) {
  auto cartridge = LoadMapper(69, 16, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr NametableMirroring kExpectedMirroring[] = {
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
  };
  for (Byte value = 0; value < std::size(kExpectedMirroring); ++value) {
    WriteRegister(mapper, 0x0c, value);
    EXPECT_EQ(mapper->GetNametableMirroring(), kExpectedMirroring[value]);
  }

  EXPECT_TRUE(mapper->NeedsM2CycleIRQ());
  WriteRegister(mapper, 0x0e, 1);
  WriteRegister(mapper, 0x0f, 0);
  WriteRegister(mapper, 0x0d, 0x81);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  WriteRegister(mapper, 0x0e, 0);
  WriteRegister(mapper, 0x0f, 0);
  WriteRegister(mapper, 0x0d, 0x80);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper069Test, PersistsRegistersIRQWorkRAMAndCHRRAM) {
  auto cartridge = LoadMapper(69, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 0x09, 3);
  WriteRegister(mapper, 0x08, 0xc2);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  WriteRegister(mapper, 0x0c, 2);
  WriteRegister(mapper, 0x0e, 1);
  WriteRegister(mapper, 0x0f, 0);
  WriteRegister(mapper, 0x0d, 0x81);
  mapper->M2CycleIRQ();
  mapper->WriteCHR(0x0123, 0x45);
  const Bytes state = SerializeMapper(mapper);

  WriteRegister(mapper, 0x09, 1);
  WriteRegister(mapper, 0x08, 0xc2);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
  WriteRegister(mapper, 0x0c, 0);
  WriteRegister(mapper, 0x0d, 0);
  mapper->WriteCHR(0x0123, 0x67);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper069Test, RendersAllAcceptanceROMs) {
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
    if (mapper_id != 69)
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

  EXPECT_EQ(verified_roms, 12u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
