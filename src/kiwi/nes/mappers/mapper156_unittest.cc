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
constexpr size_t k1K = 0x0400;
constexpr Byte kPRGBanks = 8;
constexpr Byte kCHRBanks = 64;

void SetPRGBankMarker(Bytes* rom, size_t bank, Address offset, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k16K + (offset & 0x3fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

void SetCHRBankMarker(Bytes* rom, size_t bank, Address offset, Byte value) {
  ASSERT_TRUE(rom);
  const size_t chr_start =
      kINESHeaderSize + static_cast<size_t>(kPRGBanks) * k16K;
  const size_t index = chr_start + bank * k1K + (offset & 0x03ff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper156Test : public MapperTest {
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

TEST_F(Mapper156Test, CreatesDaouBoardWithResetLayoutAndWorkRAM) {
  ASSERT_TRUE(Mapper::IsMapperSupported(156));
  auto cartridge = LoadMapper(156, kPRGBanks, kCHRBanks, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0056));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1d23), 0x0123u);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  EXPECT_TRUE(mapper->HasPRGRAM());
}

TEST_F(Mapper156Test, SelectsLower16KProgramBankAndKeepsLastBankFixed) {
  Bytes rom = MakeTestROM(156, kPRGBanks, kCHRBanks);
  SetPRGBankMarker(&rom, 5, 0x0123, 0x55);
  SetPRGBankMarker(&rom, 7, 0x0123, 0x77);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc010, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x55);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 0x77);

  mapper->WritePRG(0xc011, 2);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x55);
}

TEST_F(Mapper156Test, CombinesLowAndHighCHRRegistersForAllEightWindows) {
  Bytes rom = MakeTestROM(156, kPRGBanks, kCHRBanks);
  for (size_t slot = 0; slot < 8; ++slot) {
    const size_t bank = ((slot & 1) << 8) | (0x34 + slot);
    SetCHRBankMarker(&rom, bank, 0x0123, static_cast<Byte>(0xa0 + slot));
  }
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Address slot = 0; slot < 8; ++slot) {
    const Address low_address =
        static_cast<Address>((slot < 4 ? 0xc000 : 0xc004) + slot);
    const Address high_address =
        static_cast<Address>((slot < 4 ? 0xc004 : 0xc008) + slot);
    mapper->WritePRG(low_address, static_cast<Byte>(0x34 + slot));
    mapper->WritePRG(high_address, (slot & 1) ? 0xff : 0x00);
  }

  for (Address slot = 0; slot < 8; ++slot) {
    const size_t bank = ((slot & 1) << 8) | (0x34 + slot);
    const Address ppu_address = static_cast<Address>(slot * k1K + 0x0123);
    EXPECT_EQ(mapper->ReadCHR(ppu_address), 0xa0 + slot);
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(ppu_address), bank * k1K + 0x0123);
  }
}

TEST_F(Mapper156Test, SelectsVerticalHorizontalAndOneScreenMirroring) {
  auto cartridge = LoadMapper(156, kPRGBanks, kCHRBanks);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc014, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xc014, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0xc014, 2);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  mapper->WritePRG(0xc014, 3);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  EXPECT_EQ(mirroring_changes_, 3);
}

TEST_F(Mapper156Test, RestoresBanksMirroringAndWorkRAMThenResetsRegisters) {
  Bytes rom = MakeTestROM(156, kPRGBanks, kCHRBanks);
  SetPRGBankMarker(&rom, 6, 0x0123, 0x66);
  SetCHRBankMarker(&rom, 0x155, 0x0123, 0xd5);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc010, 6);
  mapper->WritePRG(0xc00a, 0x55);
  mapper->WritePRG(0xc00e, 1);
  mapper->WritePRG(0xc014, 1);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xc010, 2);
  mapper->WritePRG(0xc00a, 0);
  mapper->WritePRG(0xc00e, 0);
  mapper->WritePRG(0xc014, 0);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x66);
  EXPECT_EQ(mapper->ReadCHR(0x1923), 0xd5);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x1923), TestCHRByte(0x0123));
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
}

TEST_F(Mapper156Test, CorrectsAndRendersBuzzAndWaldogFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Buzz & Waldog (USA) (Unl) (Proto).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->crc, 0xccc03440u) << entry.path();
    ASSERT_EQ(emulator_->GetRomData()->mapper, 156) << entry.path();

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
