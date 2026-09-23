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
constexpr size_t k16K = 0x4000;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k16K + (address & 0x3fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper094Test : public MapperTest {
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

TEST_F(Mapper094Test, DecodesShiftedProgramBankAndKeepsLastBankFixed) {
  ASSERT_TRUE(Mapper::IsMapperSupported(94));
  Bytes rom = MakeTestROM(94, 8, 0, 0x01);
  SetPRGByte(&rom, 7, 0xff00, 0x17);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->WritePRG(0xff00, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  mapper->WritePRG(0x7fff, 0x1c);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
}

TEST_F(Mapper094Test, AppliesProgramROMBusConflicts) {
  Bytes rom = MakeTestROM(94, 8, 0);
  SetPRGByte(&rom, 7, 0xff00, 0x0c);
  SetPRGByte(&rom, 7, 0xff01, 0x14);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xff01, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));

  mapper->WritePRG(0xff00, 0x1c);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k16K + 0x0123));
}

TEST_F(Mapper094Test, PersistsProgramBankAndCharacterRAMThenResets) {
  Bytes rom = MakeTestROM(94, 8, 0);
  SetPRGByte(&rom, 7, 0xff00, 0xff);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xff00, 0x10);
  mapper->WriteCHR(0x0456, 0xa5);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xff00, 0x04);
  mapper->WriteCHR(0x0456, 0x5a);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(4 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa5);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa5);
}

TEST_F(Mapper094Test, RendersSenjouNoOokami) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Senjou no Ookami (Japan).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 94) << entry.path();

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
