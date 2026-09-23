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
#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t kINESHeaderSize = 0x10;
constexpr size_t k16K = 0x4000;
constexpr size_t k8K = 0x2000;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k16K + (address & 0x3fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

Bytes MakeWritableRegisterROM() {
  Bytes rom = MakeTestROM(92, 16, 16, 0x01);
  SetPRGByte(&rom, 0, 0x80ff, 0xff);
  return rom;
}

}  // namespace

class Mapper092Test : public MapperTest {
 protected:
  scoped_refptr<Cartridge> LoadCustomROM(const Bytes& rom) {
    auto cartridge = base::MakeRefCounted<Cartridge>(
        static_cast<EmulatorImpl*>(emulator_.get()));
    const Cartridge::LoadResult result = cartridge->Load(rom);
    EXPECT_TRUE(result.success);
    if (!result.success)
      return nullptr;

    cartridge->mapper()->Reset();
    return cartridge;
  }
};

TEST_F(Mapper092Test, CreatesJalecoJF19WithResetLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(92));
  auto cartridge = LoadMapper(92, 16, 16, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_FALSE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(15 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper092Test, AppliesPRGROMBusConflictsBeforeLatchingBanks) {
  Bytes rom = MakeTestROM(92, 16, 16);
  SetPRGByte(&rom, 0, 0x80c1, 0xc1);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80c1, 0xcf);

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(k8K + 0x0456));
}

TEST_F(Mapper092Test, LatchesFourBitBanksOnlyOnRisingEdges) {
  auto cartridge = LoadCustomROM(MakeWritableRegisterROM());
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0x8e);
  mapper->WritePRG(0x80ff, 0x83);
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(14 * k16K + 0x0123));

  mapper->WritePRG(0x80ff, 0x03);
  mapper->WritePRG(0x80ff, 0x83);
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k16K + 0x0123));

  mapper->WritePRG(0x80ff, 0x4e);
  mapper->WritePRG(0x80ff, 0x42);
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(14 * k8K + 0x0456));

  mapper->WritePRG(0x80ff, 0x02);
  mapper->WritePRG(0x80ff, 0x42);
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * k8K + 0x0456));
}

TEST_F(Mapper092Test, ResetsAndRestoresBanksAndEdgeLatchState) {
  auto cartridge = LoadCustomROM(MakeWritableRegisterROM());
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0xc5);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x80ff, 0x00);
  mapper->WritePRG(0x80ff, 0xc2);
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(2 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * k8K + 0x0456));

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  mapper->WritePRG(0x80ff, 0xc7);
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(5 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(5 * k8K + 0x0456));

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(15 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));

  mapper->WritePRG(0x80ff, 0xc2);
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(2 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * k8K + 0x0456));
}

TEST_F(Mapper092Test, RendersAllAcceptanceROMsFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, uint32_t> expected_roms = {
      {"Moero!! Pro Soccer (Japan).nes", 0x7f45cff5u},
      {"Moero!! Pro Yakyuu '88 - Ketteiban (Japan).nes", 0xb297b5e7u},
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file())
      continue;

    const auto expected = expected_roms.find(entry.path().filename().string());
    if (expected == expected_roms.end())
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    ASSERT_EQ(emulator_->GetRomData()->crc, expected->second) << entry.path();
    ASSERT_EQ(emulator_->GetRomData()->mapper, 92) << entry.path();

    emulator_->Run();
    Colors previous_pixels;
    bool frame_changed = false;
    bool final_frame_rendered = false;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      const std::set<Color> unique_colors(pixels.begin(), pixels.end());
      final_frame_rendered = unique_colors.size() > 1;
      if (!previous_pixels.empty() && pixels != previous_pixels)
        frame_changed = true;
      previous_pixels = pixels;
    }
    EXPECT_TRUE(final_frame_rendered) << entry.path();
    EXPECT_TRUE(frame_changed) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, expected_roms.size());
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
