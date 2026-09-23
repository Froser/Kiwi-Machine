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

void WriteNibbleRegister(Mapper* mapper, Address address, Byte value) {
  mapper->WritePRG(address, value & 0x0f);
  mapper->WritePRG(address + 1, value >> 4);
}

}  // namespace

class Mapper018Test : public MapperTest {};

TEST_F(Mapper018Test, MapsAllPRGAndCHRWindows) {
  auto cartridge = LoadMapper(18, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteNibbleRegister(mapper, 0x8000, 0x13);
  WriteNibbleRegister(mapper, 0x8002, 0x04);
  WriteNibbleRegister(mapper, 0x9000, 0x1d);

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0x13 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(0x04 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(0x1d * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(0x1f * k8K));

  constexpr Address kCHRRegisters[] = {
      0xa000, 0xa002, 0xb000, 0xb002, 0xc000, 0xc002, 0xd000, 0xd002,
  };
  for (size_t bank = 0; bank < std::size(kCHRRegisters); ++bank)
    WriteNibbleRegister(mapper, kCHRRegisters[bank], 0x20 + bank);

  for (size_t bank = 0; bank < std::size(kCHRRegisters); ++bank) {
    const Address address = static_cast<Address>(bank * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte((0x20 + bank) * k1K + 0x123));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address),
              (0x20 + bank) * k1K + 0x123);
  }
}

TEST_F(Mapper018Test, ControlsAllMirroringModesAndResetsToHeader) {
  auto cartridge = LoadMapper(18, 8, 16, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xf002, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0xf002, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xf002, 2);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  mapper->WritePRG(0xf002, 3);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
  EXPECT_EQ(mirroring_changes_, 4);

  mapper->Reset();
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper018Test, ClocksSelectableWidthIRQCounterOnM2Cycles) {
  auto cartridge = LoadMapper(18, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  EXPECT_TRUE(mapper->NeedsM2CycleIRQ());

  mapper->WritePRG(0xe000, 3);
  mapper->WritePRG(0xf000, 0);
  mapper->WritePRG(0xf001, 1);
  mapper->M2CycleIRQ();
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0xe000, 2);
  mapper->WritePRG(0xe001, 1);
  mapper->WritePRG(0xf000, 0);
  mapper->WritePRG(0xf001, 0x09);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);

  mapper->WritePRG(0xf001, 0);
  for (int cycle = 0; cycle < 32; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);
}

TEST_F(Mapper018Test, PersistsRegistersIRQAndCHRRAM) {
  auto cartridge = LoadMapper(18, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteNibbleRegister(mapper, 0x8000, 3);
  mapper->WritePRG(0xf002, 2);
  mapper->WritePRG(0xe000, 3);
  mapper->WritePRG(0xf000, 0);
  mapper->WritePRG(0xf001, 1);
  mapper->M2CycleIRQ();
  mapper->WriteCHR(0x0123, 0x45);
  const Bytes state = SerializeMapper(mapper);

  WriteNibbleRegister(mapper, 0x8000, 1);
  mapper->WritePRG(0xf001, 0);
  mapper->WritePRG(0xf002, 0);
  mapper->WriteCHR(0x0123, 0x67);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);

  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper018Test, RendersAllAcceptanceROMs) {
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
    if (mapper_id != 18)
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

  EXPECT_EQ(verified_roms, 17u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
