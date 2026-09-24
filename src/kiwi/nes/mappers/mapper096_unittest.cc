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
#include <map>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/emulator_impl.h"
#include "nes/ppu_bus.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t kINESHeaderSize = 0x10;
constexpr size_t k32K = 0x8000;
constexpr size_t k4K = 0x1000;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k32K + (address & 0x7fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

Bytes MakeWritableRegisterROM() {
  Bytes rom = MakeTestROM(96, 8, 0);
  for (size_t bank = 0; bank < 4; ++bank)
    SetPRGByte(&rom, bank, 0x80ff, 0xff);
  return rom;
}

void SelectInnerCHRBank(Mapper* mapper, Byte bank) {
  mapper->PPUAddressChanged(0x0000);
  mapper->PPUAddressChanged(static_cast<Address>(0x2000 | (bank << 8)));
}

}  // namespace

class Mapper096Test : public MapperTest {
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

TEST_F(Mapper096Test, CreatesOekaKidsWithResetLayoutAnd32KCharacterRAM) {
  ASSERT_TRUE(Mapper::IsMapperSupported(96));
  auto cartridge = LoadMapper(96, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_FALSE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xf456), TestPRGByte(0x7456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 0x0123u);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1123), 3u * k4K + 0x0123);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper096Test, SelectsOuterBanksAndAppliesProgramROMBusConflicts) {
  auto cartridge = LoadCustomROM(MakeWritableRegisterROM());
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Byte value = 0; value < 8; ++value) {
    mapper->WritePRG(0x80ff, value);
    EXPECT_EQ(mapper->ReadPRG(0x8123),
              TestPRGByte((value & 0x03) * k32K + 0x0123));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123),
              (value & 0x04) * k4K + 0x0123);
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1123),
              ((value & 0x04) | 0x03) * k4K + 0x0123);
  }

  Bytes conflicted_rom = MakeTestROM(96, 8, 0);
  SetPRGByte(&conflicted_rom, 0, 0x80ff, 0x52);
  cartridge = LoadCustomROM(conflicted_rom);
  ASSERT_TRUE(cartridge);
  mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0x73);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k32K + 0x0123));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 0x0123u);
}

TEST_F(Mapper096Test, LatchesInnerBankOnlyWhenEnteringNametableRange) {
  auto cartridge = LoadCustomROM(MakeWritableRegisterROM());
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  SelectInnerCHRBank(mapper, 2);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 2u * k4K + 0x0123);

  mapper->PPUAddressChanged(0x2300);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 2u * k4K + 0x0123);

  mapper->PPUAddressChanged(0x3000);
  mapper->PPUAddressChanged(0x2100);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), k4K + 0x0123);

  mapper->PPUAddressChanged(0x3f00);
  mapper->PPUAddressChanged(0x2f00);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 3u * k4K + 0x0123);
}

TEST_F(Mapper096Test, RoutesPPUAccessesToDynamicCharacterRAMBanks) {
  auto cartridge = LoadCustomROM(MakeWritableRegisterROM());
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  for (Byte bank = 0; bank < 4; ++bank) {
    ppu_bus.SetCurrentPatternState(PPUBus::CurrentPatternType::kBackground,
                                   false, 0,
                                   static_cast<Address>(0x2000 | (bank << 8)));
    ppu_bus.Write(0x0123, static_cast<Byte>(0xa0 + bank));
  }
  for (Byte bank = 0; bank < 4; ++bank) {
    ppu_bus.SetCurrentPatternState(PPUBus::CurrentPatternType::kBackground,
                                   false, 0,
                                   static_cast<Address>(0x2000 | (bank << 8)));
    EXPECT_EQ(ppu_bus.Read(0x0123), 0xa0 + bank);
  }

  SelectInnerCHRBank(mapper, 2);
  ppu_bus.Read(0x0123);
  ppu_bus.SetCurrentPatternState(PPUBus::CurrentPatternType::kSprite, false, 0);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 0x0123u);

  ppu_bus.Write(0x2000, 0x11);
  EXPECT_EQ(ppu_bus.Read(0x2800), 0x11);
  ppu_bus.Write(0x2400, 0x22);
  EXPECT_EQ(ppu_bus.Read(0x2c00), 0x22);
}

TEST_F(Mapper096Test, RestoresBanksAddressLatchAndCharacterRAMThenResets) {
  auto cartridge = LoadCustomROM(MakeWritableRegisterROM());
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0x06);
  SelectInnerCHRBank(mapper, 2);
  mapper->WriteCHR(0x0123, 0x5a);
  mapper->PPUAddressChanged(0x2200);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x80ff, 0x00);
  SelectInnerCHRBank(mapper, 1);
  mapper->WriteCHR(0x0123, 0xa5);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);

  mapper->PPUAddressChanged(0x2300);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 6u * k4K + 0x0123);
  mapper->PPUAddressChanged(0x0000);
  mapper->PPUAddressChanged(0x2100);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 5u * k4K + 0x0123);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 0x0123u);
  mapper->WritePRG(0x80ff, 0x04);
  SelectInnerCHRBank(mapper, 2);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
}

TEST_F(Mapper096Test, RendersBothOekaKidsDumpsFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, uint32_t> expected_roms{
      {"Oeka Kids - Anpanman no Hiragana Daisuki (Japan).nes", 0xc3c0811du},
      {"Oeka Kids - Anpanman to Oekaki Shiyou!! (Japan).nes", 0x9d048ea4u},
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 96) << entry.path();

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
