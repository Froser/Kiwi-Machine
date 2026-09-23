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

constexpr size_t k16K = 0x4000;
constexpr size_t k1K = 0x0400;

void WriteLines(Mapper* mapper, Byte scl, Byte sda) {
  mapper->WritePRG(0x800d, static_cast<Byte>((scl << 5) | (sda << 6)));
}

void StartI2C(Mapper* mapper) {
  WriteLines(mapper, 0, 1);
  WriteLines(mapper, 1, 1);
  WriteLines(mapper, 1, 0);
}

void WriteI2CBit(Mapper* mapper, Byte value) {
  WriteLines(mapper, 0, value);
  WriteLines(mapper, 1, value);
}

void WriteI2CByte(Mapper* mapper, Byte value) {
  for (int bit = 7; bit >= 0; --bit)
    WriteI2CBit(mapper, (value >> bit) & 0x01);
}

void ClockI2CAcknowledge(Mapper* mapper) {
  WriteLines(mapper, 0, 1);
  WriteLines(mapper, 1, 1);
  WriteLines(mapper, 0, 1);
}

void StopI2C(Mapper* mapper) {
  WriteLines(mapper, 0, 0);
  WriteLines(mapper, 1, 0);
  WriteLines(mapper, 1, 1);
}

}  // namespace

class Mapper016Test : public MapperTest {};

TEST_F(Mapper016Test, MapsFCGRegistersInSixThousandRange) {
  auto cartridge = LoadMapper(16, 16, 16, 0, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6008, 3);
  mapper->WritePRG(0x8008, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(15 * k16K));

  for (Byte window = 0; window < 8; ++window)
    mapper->WriteExtendedRAM(0x6000 + window, 0x10 + window);
  for (size_t window = 0; window < 8; ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte((0x10 + window) * k1K + 0x123));
  }

  mapper->WriteExtendedRAM(0x600b, 1);
  mapper->WriteExtendedRAM(0x600c, 0);
  mapper->WriteExtendedRAM(0x600a, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper016Test, MapsLZ93D50RegistersAndReloadsIRQ) {
  auto cartridge = LoadMapper(16, 16, 16, 0, 5);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6008, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  mapper->WritePRG(0x8008, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k16K));

  mapper->WritePRG(0x800b, 1);
  mapper->WritePRG(0x800c, 0);
  mapper->WritePRG(0x800a, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper016Test, MapsMapper153OuterPRGBankAndGatedSaveRAM) {
  auto cartridge = LoadMapper(153, 32, 0, 0x02, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8008, 2);
  mapper->WritePRG(0x8000, 1);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0x12 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(0x1f * k16K));

  mapper->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
  mapper->WritePRG(0x800d, 0x20);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0xff);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  mapper->WritePRG(0x800d, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
}

TEST_F(Mapper016Test, SupportsDatachAndMapper159BoardVariants) {
  auto datach = LoadMapper(157, 16, 0);
  ASSERT_TRUE(datach);
  datach->mapper()->WritePRG(0x8008, 4);
  datach->mapper()->WriteCHR(0x0123, 0x5a);
  EXPECT_EQ(datach->mapper()->ReadPRG(0x8000), TestPRGByte(4 * k16K));
  EXPECT_EQ(datach->mapper()->ReadCHR(0x0123), 0x5a);
  ASSERT_TRUE(datach->mapper()->ExportPRGNVRAM(false));
  EXPECT_EQ(datach->mapper()->ExportPRGNVRAM(false)->data.size(), 256u);

  auto mapper159 = LoadMapper(159, 16, 16);
  ASSERT_TRUE(mapper159);
  mapper159->mapper()->WritePRG(0x8000, 7);
  EXPECT_EQ(mapper159->mapper()->ReadCHR(0), TestCHRByte(7 * k1K));
  ASSERT_TRUE(mapper159->mapper()->ExportPRGNVRAM(false));
  EXPECT_EQ(mapper159->mapper()->ExportPRGNVRAM(false)->data.size(), 128u);
}

TEST_F(Mapper016Test, Writes24C02EEPROMAndPersistsState) {
  auto cartridge = LoadMapper(16, 16, 16, 0, 5, 0, 0x20);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  StartI2C(mapper);
  WriteI2CByte(mapper, 0xa0);
  ClockI2CAcknowledge(mapper);
  WriteI2CByte(mapper, 0x2a);
  ClockI2CAcknowledge(mapper);
  WriteI2CByte(mapper, 0x5a);
  ClockI2CAcknowledge(mapper);
  StopI2C(mapper);

  const auto snapshot = mapper->ExportPRGNVRAM(true);
  ASSERT_TRUE(snapshot);
  ASSERT_EQ(snapshot->data.size(), 256u);
  EXPECT_EQ(snapshot->data[0x2a], 0x5a);

  const Bytes state = SerializeMapper(mapper);
  Bytes replacement(256, 0xa5);
  ASSERT_TRUE(mapper->ImportPRGNVRAM(replacement));
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  const auto restored = mapper->ExportPRGNVRAM(false);
  ASSERT_TRUE(restored);
  EXPECT_EQ(restored->data[0x2a], 0x5a);
}

TEST_F(Mapper016Test, RendersAllAcceptanceROMs) {
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
    if (mapper_id != 16 && mapper_id != 153 && mapper_id != 157 &&
        mapper_id != 159) {
      continue;
    }

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

  EXPECT_EQ(verified_roms, 28u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
