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

}  // namespace

class Mapper023Test : public MapperTest {};

TEST_F(Mapper023Test, MapsVRC2bBanksAndMicrowireLatch) {
  auto cartridge = LoadMapper(23, 8, 16, 0, 3);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0xa000, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(15 * k8K));

  mapper->WritePRG(0xb000, 0x05);
  mapper->WritePRG(0xb001, 0x01);
  mapper->WritePRG(0xb002, 0x06);
  mapper->WritePRG(0xb003, 0x02);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x15 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadCHR(0x0523), TestCHRByte(0x26 * k1K + 0x123));

  mapper->WritePRG(0x9000, 3);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_FALSE(mapper->NeedsM2CycleIRQ());

  mapper->WriteExtendedRAM(0x6123, 1);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
  mapper->WriteExtendedRAM(0x6200, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6200), 0x62);
}

TEST_F(Mapper023Test, MapsVRC4eBanksAndPRGMode) {
  auto cartridge = LoadMapper(23, 8, 16, 0, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0xa000, 4);
  mapper->WritePRG(0x9008, 2);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(15 * k8K));

  mapper->WritePRG(0xb000, 0x05);
  mapper->WritePRG(0xb004, 0x01);
  mapper->WritePRG(0xb008, 0x06);
  mapper->WritePRG(0xb00c, 0x02);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x15 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadCHR(0x0523), TestCHRByte(0x26 * k1K + 0x123));

  mapper->WritePRG(0x9000, 2);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  EXPECT_TRUE(mapper->NeedsM2CycleIRQ());
}

TEST_F(Mapper023Test, SupportsVRC4CycleAndScanlineIRQModes) {
  auto cartridge = LoadMapper(23, 8, 16, 0, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xf000, 0x0e);
  mapper->WritePRG(0xf004, 0x0f);
  mapper->WritePRG(0xf008, 0x06);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->WritePRG(0xf00c, 0);
  for (int cycle = 0; cycle < 4; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0xf000, 0x0f);
  mapper->WritePRG(0xf004, 0x0f);
  mapper->WritePRG(0xf008, 0x02);
  for (int cycle = 0; cycle < 113; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);
}

TEST_F(Mapper023Test, PersistsRegistersIRQRAMAndCHRRAM) {
  auto cartridge = LoadMapper(23, 8, 0, 0, 2, 0, 0x05);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0x9000, 2);
  mapper->WritePRG(0xf000, 0x0e);
  mapper->WritePRG(0xf004, 0x0f);
  mapper->WritePRG(0xf008, 0x06);
  mapper->M2CycleIRQ();
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  mapper->WriteCHR(0x0123, 0x45);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0x9000, 0);
  mapper->WritePRG(0xf00c, 0);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
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

TEST_F(Mapper023Test, RendersAllAcceptanceROMs) {
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
    if (mapper_id != 23)
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
