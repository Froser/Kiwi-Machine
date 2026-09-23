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
constexpr size_t k8K = 0x2000;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k32K + (address & (k32K - 1));
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper234Test : public MapperTest {
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

TEST_F(Mapper234Test, CreatesMaxi15WithInitialLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(234));
  auto cartridge = LoadMapper(234, 32, 64, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_FALSE(mapper->HasPRGRAM());
}

TEST_F(Mapper234Test, ReadOuterRegisterLatchesROMValueAndLocksSelection) {
  Bytes rom = MakeTestROM(234, 32, 64);
  SetPRGByte(&rom, 0, 0xff80, 0x05);
  SetPRGByte(&rom, 5, 0xff81, 0x02);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0xff80), 0x05);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(20 * k8K + 0x0456));

  EXPECT_EQ(mapper->ReadPRG(0xff81), 0x02);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k32K + 0x0123));
}

TEST_F(Mapper234Test, OuterRegisterStaysWritableUntilSelectionBitsAreSet) {
  Bytes rom = MakeTestROM(234, 32, 64);
  SetPRGByte(&rom, 0, 0xff80, 0xc0);
  SetPRGByte(&rom, 0, 0xff81, 0x82);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0xff80), 0xc0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);

  EXPECT_EQ(mapper->ReadPRG(0xff81), 0x82);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(8 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper234Test, InnerRegisterSelectsCNROMCharacterPage) {
  Bytes rom = MakeTestROM(234, 32, 64);
  SetPRGByte(&rom, 0, 0xff80, 0x05);
  SetPRGByte(&rom, 5, 0xffe8, 0x30);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0xff80), 0x05);
  EXPECT_EQ(mapper->ReadPRG(0xffe8), 0x30);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(23 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 23u * k8K + 0x0456);
}

TEST_F(Mapper234Test, InnerRegisterSelectsNINA03ProgramAndCharacterPages) {
  Bytes rom = MakeTestROM(234, 32, 64);
  SetPRGByte(&rom, 0, 0xff80, 0x46);
  SetPRGByte(&rom, 6, 0xffe8, 0x71);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0xff80), 0x46);
  EXPECT_EQ(mapper->ReadPRG(0xffe8), 0x71);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(7 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(31 * k8K + 0x0456));
}

TEST_F(Mapper234Test, RegisterWritesApplyPRGROMBusConflicts) {
  Bytes rom = MakeTestROM(234, 32, 64);
  SetPRGByte(&rom, 0, 0xff80, 0x0f);
  SetPRGByte(&rom, 5, 0xffe8, 0x31);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xff80, 0xf5);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k32K + 0x0123));

  mapper->WritePRG(0xffe8, 0x70);
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(23 * k8K + 0x0456));
}

TEST_F(Mapper234Test, IgnoresReadsOutsideRegisterWindows) {
  Bytes rom = MakeTestROM(234, 32, 64);
  SetPRGByte(&rom, 0, 0xffa0, 0x0f);
  SetPRGByte(&rom, 0, 0xffc0, 0x0f);
  SetPRGByte(&rom, 0, 0xffe7, 0x0f);
  SetPRGByte(&rom, 0, 0xfff8, 0x0f);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0xffa0), 0x0f);
  EXPECT_EQ(mapper->ReadPRG(0xffc0), 0x0f);
  EXPECT_EQ(mapper->ReadPRG(0xffe7), 0x0f);
  EXPECT_EQ(mapper->ReadPRG(0xfff8), 0x0f);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
}

TEST_F(Mapper234Test, ResetsAndRestoresRegisters) {
  Bytes rom = MakeTestROM(234, 32, 64);
  SetPRGByte(&rom, 0, 0xff80, 0xc0);
  SetPRGByte(&rom, 0, 0xffe8, 0x51);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0xff80), 0xc0);
  EXPECT_EQ(mapper->ReadPRG(0xffe8), 0x51);
  const Bytes state = SerializeMapper(mapper);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(5 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper234Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Maxi 15 (USA) (Rev 1) (Unl).nes",
      "Maxi 15 (USA) (Unl) (Rev 1).nes",
      "Maxi 15 (USA) (Unl).nes",
      "Maxi-15 Pack (Australia) (Unl).nes",
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() ||
        !expected_roms.contains(entry.path().filename().string())) {
      continue;
    }

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    ASSERT_EQ(emulator_->GetRomData()->mapper, 234) << entry.path();

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
