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

void SetPRGByte(Bytes* rom, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + address - 0x8000;
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper013Test : public MapperTest {
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

TEST_F(Mapper013Test, MapsFixedProgramROMAndUsesVerticalMirroring) {
  ASSERT_TRUE(Mapper::IsMapperSupported(13));
  auto cartridge = LoadMapper(13, 2, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper013Test, MapsFourCharacterRAMPagesIntoUpperPatternTable) {
  Bytes rom = MakeTestROM(13, 2, 0);
  for (Byte bank = 0; bank < 4; ++bank)
    SetPRGByte(&rom, static_cast<Address>(0x8100 + bank), bank);

  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0456, 0xa0);
  for (Byte bank = 1; bank < 4; ++bank) {
    mapper->WritePRG(static_cast<Address>(0x8100 + bank), 0xff);
    mapper->WriteCHR(0x1456, static_cast<Byte>(0xa0 + bank));
  }

  for (Byte bank = 0; bank < 4; ++bank) {
    mapper->WritePRG(static_cast<Address>(0x8100 + bank), 0xff);
    EXPECT_EQ(mapper->ReadCHR(0x1456), static_cast<Byte>(0xa0 + bank));
  }
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa0);

  mapper->WritePRG(0x8103, 0xff);
  mapper->WritePRG(0x7fff, 0);
  EXPECT_EQ(mapper->ReadCHR(0x1456), 0xa3);
}

TEST_F(Mapper013Test, AppliesProgramROMBusConflicts) {
  Bytes rom = MakeTestROM(13, 2, 0);
  SetPRGByte(&rom, 0x8123, 0x02);
  SetPRGByte(&rom, 0x8124, 0x03);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8124, 0xff);
  mapper->WriteCHR(0x1555, 0x33);
  mapper->WritePRG(0x8123, 0x03);
  mapper->WriteCHR(0x1555, 0x22);
  mapper->WritePRG(0x8124, 0xff);

  EXPECT_EQ(mapper->ReadCHR(0x1555), 0x33);
  mapper->WritePRG(0x8123, 0x03);
  EXPECT_EQ(mapper->ReadCHR(0x1555), 0x22);
}

TEST_F(Mapper013Test, PersistsSelectedPageAndAllCharacterRAM) {
  Bytes rom = MakeTestROM(13, 2, 0);
  SetPRGByte(&rom, 0x80ff, 0xff);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0456, 0x10);
  mapper->WritePRG(0x80ff, 2);
  mapper->WriteCHR(0x1456, 0x22);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteCHR(0x0456, 0x40);
  mapper->WritePRG(0x80ff, 3);
  mapper->WriteCHR(0x1456, 0x33);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0x10);
  EXPECT_EQ(mapper->ReadCHR(0x1456), 0x22);

  mapper->Reset();
  mapper->WritePRG(0x80ff, 2);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0x10);
  EXPECT_EQ(mapper->ReadCHR(0x1456), 0x22);
}

TEST_F(Mapper013Test, RendersVideomation) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Videomation (USA).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 13) << entry.path();

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

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
