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
constexpr size_t k32K = 0x8000;
constexpr size_t k2K = 0x0800;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k32K + (address & 0x7fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper077Test : public MapperTest {
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

TEST_F(Mapper077Test, MapsProgramAndCharacterBanksWithBusConflicts) {
  ASSERT_TRUE(Mapper::IsMapperSupported(77));
  Bytes rom = MakeTestROM(77, 8, 4);
  SetPRGByte(&rom, 0, 0x8123, 0xb2);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8123, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8456), TestPRGByte(2 * k32K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(11 * k2K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 11u * k2K + 0x0456);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kFourScreen);
}

TEST_F(Mapper077Test, SeparatesCharacterROMFromSixKiBCharacterRAM) {
  auto cartridge = LoadMapper(77, 8, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte character_rom = mapper->ReadCHR(0x0456);
  mapper->WriteCHR(0x0456, character_rom ^ 0xff);
  mapper->WriteCHR(0x0856, 0x81);
  mapper->WriteCHR(0x1056, 0x82);
  mapper->WriteCHR(0x1856, 0x83);

  EXPECT_EQ(mapper->ReadCHR(0x0456), character_rom);
  EXPECT_EQ(mapper->ReadCHR(0x0856), 0x81);
  EXPECT_EQ(mapper->ReadCHR(0x1056), 0x82);
  EXPECT_EQ(mapper->ReadCHR(0x1856), 0x83);

  Bytes ciram(0x0800);
  EXPECT_TRUE(mapper->UsesCustomPPUMemoryMapping());
  mapper->WritePPUMemoryByte(ciram.data(), 0x0856, 0x91);
  EXPECT_EQ(mapper->ReadPPUMemoryByte(ciram.data(), 0x2056), 0x91);
  mapper->WritePPUMemoryByte(ciram.data(), 0x2856, 0x92);
  EXPECT_EQ(mapper->ReadPPUMemoryByte(ciram.data(), 0x2856), 0x92);
  EXPECT_EQ(ciram[0x0056], 0x92);
}

TEST_F(Mapper077Test, PersistsBanksAndCharacterRAMThenResets) {
  Bytes rom = MakeTestROM(77, 8, 4);
  SetPRGByte(&rom, 0, 0x80ff, 0xff);
  SetPRGByte(&rom, 2, 0x80ff, 0xff);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0xb2);
  mapper->WriteCHR(0x1856, 0xa5);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x80ff, 0x01);
  mapper->WriteCHR(0x1856, 0x5a);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8456), TestPRGByte(2 * k32K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(11 * k2K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1856), 0xa5);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8456), TestPRGByte(0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1856), 0xa5);
}

TEST_F(Mapper077Test, RendersNapoleonSenki) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Napoleon Senki (Japan).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 77) << entry.path();

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
