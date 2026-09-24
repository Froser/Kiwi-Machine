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
constexpr size_t k8K = 0x2000;
constexpr size_t k2K = 0x0800;

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k8K + (address & 0x1fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper193Test : public MapperTest {
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

TEST_F(Mapper193Test, MapsProgramCharacterBanksAndMirroring) {
  ASSERT_TRUE(Mapper::IsMapperSupported(193));
  Bytes rom = MakeTestROM(193, 8, 32);
  SetPRGByte(&rom, 5, 0x8123, 0xa5);
  SetPRGByte(&rom, 13, 0xa123, 0xbd);
  SetPRGByte(&rom, 14, 0xc123, 0xbe);
  SetPRGByte(&rom, 15, 0xe123, 0xbf);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6000, 0x16);
  mapper->WriteExtendedRAM(0x6001, 0x0c);
  mapper->WriteExtendedRAM(0x6002, 0x0e);
  mapper->WriteExtendedRAM(0x6003, 5);
  mapper->WriteExtendedRAM(0x6004, 1);

  EXPECT_EQ(mapper->ReadPRG(0x8123), 0xa5);
  EXPECT_EQ(mapper->ReadPRG(0xa123), 0xbd);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 0xbe);
  EXPECT_EQ(mapper->ReadPRG(0xe123), 0xbf);
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(10 * k2K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x0c56), TestCHRByte(11 * k2K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1456), TestCHRByte(6 * k2K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1c56), TestCHRByte(7 * k2K + 0x0456));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper193Test, MirrorsRegistersWithE007Mask) {
  auto cartridge = LoadMapper(193, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x7ff8, 0x08);
  mapper->WriteExtendedRAM(0x7ff9, 0x0a);
  mapper->WriteExtendedRAM(0x7ffa, 0x0c);
  mapper->WriteExtendedRAM(0x7ffb, 3);
  mapper->WriteExtendedRAM(0x7ffc, 1);

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(4 * k2K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1456), TestCHRByte(5 * k2K + 0x0456));
  EXPECT_EQ(mapper->ReadCHR(0x1c56), TestCHRByte(6 * k2K + 0x0456));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper193Test, PersistsBanksAndKeepsCharacterROMReadOnly) {
  auto cartridge = LoadMapper(193, 8, 32, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6000, 0x14);
  mapper->WriteExtendedRAM(0x6003, 5);
  mapper->WriteExtendedRAM(0x6004, 1);
  const Byte original = mapper->ReadCHR(0x0456);
  mapper->WriteCHR(0x0456, original ^ 0xff);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x6000, 0);
  mapper->WriteExtendedRAM(0x6003, 0);
  mapper->WriteExtendedRAM(0x6004, 0);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), original);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper193Test, RendersFightingHero) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Fighting Hero (Asia) (Unl).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 193) << entry.path();

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
