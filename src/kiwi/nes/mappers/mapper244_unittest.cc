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

constexpr std::array<std::array<Byte, 4>, 4> kPRGBanks = {{
    {0, 1, 2, 3},
    {3, 2, 1, 0},
    {0, 2, 1, 3},
    {3, 1, 2, 0},
}};

constexpr std::array<std::array<Byte, 8>, 8> kCHRBanks = {{
    {0, 1, 2, 3, 4, 5, 6, 7},
    {0, 2, 1, 3, 4, 6, 5, 7},
    {0, 1, 4, 5, 2, 3, 6, 7},
    {0, 4, 1, 5, 2, 6, 3, 7},
    {0, 4, 2, 6, 1, 5, 3, 7},
    {0, 2, 4, 6, 1, 3, 5, 7},
    {7, 6, 5, 4, 3, 2, 1, 0},
    {7, 6, 5, 4, 3, 2, 1, 0},
}};

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k32K + (address & 0x7fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper244Test : public MapperTest {
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

TEST_F(Mapper244Test, AppliesEveryProgramBankPermutation) {
  ASSERT_TRUE(Mapper::IsMapperSupported(244));
  Bytes rom = MakeTestROM(244, 8, 8);
  for (size_t bank = 0; bank < 4; ++bank)
    SetPRGByte(&rom, bank, 0x8123, static_cast<Byte>(0xa0 + bank));
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Byte row = 0; row < 4; ++row) {
    for (Byte column = 0; column < 4; ++column) {
      mapper->WritePRG(0x8000, static_cast<Byte>((row << 4) | column));
      EXPECT_EQ(mapper->ReadPRG(0x8123),
                static_cast<Byte>(0xa0 + kPRGBanks[row][column]));
    }
  }
}

TEST_F(Mapper244Test, AppliesEveryCharacterBankPermutation) {
  auto cartridge = LoadMapper(244, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Byte row = 0; row < 8; ++row) {
    for (Byte column = 0; column < 8; ++column) {
      mapper->WritePRG(0x8000, static_cast<Byte>((row << 4) | 0x08 | column));
      const size_t bank = kCHRBanks[row][column];
      EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(bank * k8K + 0x0456));
      EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), bank * k8K + 0x0456);
    }
  }
}

TEST_F(Mapper244Test, PersistsBanksAndKeepsCharacterROMReadOnly) {
  Bytes rom = MakeTestROM(244, 8, 8, 0x01);
  for (size_t bank = 0; bank < 4; ++bank)
    SetPRGByte(&rom, bank, 0x8123, static_cast<Byte>(0xa0 + bank));
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x11);
  mapper->WritePRG(0x8000, 0x5d);
  const Byte original = mapper->ReadCHR(0x0456);
  mapper->WriteCHR(0x0456, original ^ 0xff);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 0x00);
  mapper->WritePRG(0x8000, 0x08);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0xa2);
  EXPECT_EQ(mapper->ReadCHR(0x0456), original);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0xa0);
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
}

TEST_F(Mapper244Test, RendersDecathlon) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Decathlon (Asia) (Unl).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 244) << entry.path();

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
