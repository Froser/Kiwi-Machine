// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

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

Bytes MakeWritableRegisterROM(Byte submapper) {
  Bytes rom = MakeTestROM(78, 8, 16, 0, submapper);
  for (size_t bank = 0; bank < 8; ++bank)
    SetPRGByte(&rom, bank, 0x80ff, 0xff);
  return rom;
}

}  // namespace

class Mapper078Test : public MapperTest {
 protected:
  scoped_refptr<Cartridge> LoadCustomROM(const Bytes& rom) {
    auto cartridge = base::MakeRefCounted<Cartridge>(
        static_cast<EmulatorImpl*>(emulator_.get()));
    const Cartridge::LoadResult result = cartridge->Load(rom);
    EXPECT_TRUE(result.success);
    if (!result.success)
      return nullptr;

    cartridge->mapper()->set_mirroring_changed_callback(base::BindRepeating(
        [](int* count) { ++*count; }, base::Unretained(&mirroring_changes_)));
    cartridge->mapper()->Reset();
    return cartridge;
  }
};

TEST_F(Mapper078Test, MapsBanksAndAppliesPRGROMBusConflicts) {
  ASSERT_TRUE(Mapper::IsMapperSupported(78));
  Bytes rom = MakeTestROM(78, 8, 16, 0, 3);
  SetPRGByte(&rom, 0, 0x80b5, 0xb5);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  mapper->WritePRG(0x80b5, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(11 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 11u * k8K + 0x0456);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper078Test, UsesSubmapperSpecificMirroring) {
  auto jf16 = LoadCustomROM(MakeWritableRegisterROM(1));
  ASSERT_TRUE(jf16);
  Mapper* mapper = jf16->mapper();

  mapper->WritePRG(0x80ff, 0x08);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
  mapper->WritePRG(0x80ff, 0x00);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);

  auto holy_diver = LoadCustomROM(MakeWritableRegisterROM(3));
  ASSERT_TRUE(holy_diver);
  mapper = holy_diver->mapper();
  mapper->WritePRG(0x80ff, 0x08);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0x80ff, 0x00);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mirroring_changes_, 4);
}

TEST_F(Mapper078Test, PersistsBanksMirroringAndCharacterRAMThenResets) {
  auto cartridge = LoadMapper(78, 8, 0, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0x25);
  mapper->WriteCHR(0x0456, 0xa5);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x80ff, 0x18);
  mapper->WriteCHR(0x0456, 0x5a);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa5);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa5);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
}

TEST_F(Mapper078Test, PatchesAndRendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, Byte> expected_roms = {
      {"Holy Diver (Japan).nes", 3},
      {"Uchuusen - Cosmo Carrier (Japan).nes", 1},
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    const auto expected = expected_roms.find(entry.path().filename().string());
    if (!entry.is_regular_file() || expected == expected_roms.end())
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    EXPECT_EQ(emulator_->GetRomData()->mapper, 78) << entry.path();
    EXPECT_EQ(emulator_->GetRomData()->submapper, expected->second)
        << entry.path();
    EXPECT_EQ(emulator_->GetRomData()->name_table_mirroring,
              NametableMirroring::kHorizontal)
        << entry.path();

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
