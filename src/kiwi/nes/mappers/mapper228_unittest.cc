// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

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
constexpr size_t k8K = 0x2000;
constexpr Byte kAction52PRGBanks = 96;
constexpr Byte kAction52CHRBanks = 64;

void SetPRGBankMarker(Bytes* rom, size_t bank, Address offset, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k16K + (offset & 0x3fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

void SetCHRBankMarker(Bytes* rom, size_t bank, Address offset, Byte value) {
  ASSERT_TRUE(rom);
  const size_t chr_start =
      kINESHeaderSize + static_cast<size_t>(kAction52PRGBanks) * k16K;
  const size_t index = chr_start + bank * k8K + (offset & 0x1fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

}  // namespace

class Mapper228Test : public MapperTest {
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

TEST_F(Mapper228Test, CreatesActionEnterprisesMapperWithResetLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(228));
  Bytes rom = MakeTestROM(228, kAction52PRGBanks, kAction52CHRBanks);
  SetPRGBankMarker(&rom, 0, 0x0123, 0x10);
  SetPRGBankMarker(&rom, 1, 0x0123, 0x11);
  SetCHRBankMarker(&rom, 0, 0x0456, 0x80);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x10);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 0x11);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0x80);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper228Test, SelectsPRGFromAddressAndCHRFromAddressAndData) {
  Bytes rom = MakeTestROM(228, kAction52PRGBanks, kAction52CHRBanks);
  SetPRGBankMarker(&rom, 36, 0x0123, 0x44);
  SetPRGBankMarker(&rom, 37, 0x0123, 0x45);
  SetCHRBankMarker(&rom, 38, 0x0456, 0xa6);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8949, 0x02);

  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x44);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 0x45);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa6);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 38u * k8K + 0x0456);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper228Test, MirrorsSelectedPRGPageAndMapsPackedThirdChip) {
  Bytes rom = MakeTestROM(228, kAction52PRGBanks, kAction52CHRBanks);
  SetPRGBankMarker(&rom, 71, 0x0123, 0x77);
  SetCHRBankMarker(&rom, 43, 0x0456, 0xab);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xb9ea, 0x03);

  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x77);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 0x77);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xab);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mirroring_changes_, 1);
}

TEST_F(Mapper228Test, LeavesMissingSecondPRGChipOpenBus) {
  Bytes rom = MakeTestROM(228, kAction52PRGBanks, kAction52CHRBanks);
  SetPRGBankMarker(&rom, 0, 0x0123, 0x10);
  SetPRGBankMarker(&rom, 1, 0x0123, 0x11);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4020, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x10);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 0x11);

  mapper->WritePRG(0x9000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x81);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 0xc1);
}

TEST_F(Mapper228Test, KeepsCHRReadOnlyAndRestoresLatchedState) {
  Bytes rom = MakeTestROM(228, kAction52PRGBanks, kAction52CHRBanks);
  SetPRGBankMarker(&rom, 36, 0x0123, 0x44);
  SetPRGBankMarker(&rom, 37, 0x0123, 0x45);
  SetCHRBankMarker(&rom, 38, 0x0456, 0xa6);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8949, 0x02);
  const Byte original = mapper->ReadCHR(0x0456);
  mapper->WriteCHR(0x0456, original ^ 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x0456), original);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0xb9ea, 0x03);
  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), 0x44);
  EXPECT_EQ(mapper->ReadPRG(0xc123), 0x45);
  EXPECT_EQ(mapper->ReadCHR(0x0456), 0xa6);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper228Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Action 52 (USA) (Rev A) (Unl).nes",
      "Action 52 (USA) (Unl) (Rev A).nes",
      "Action 52 (USA) (Unl).nes",
      "Cheetahmen II (USA) (Unl).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 228) << entry.path();

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
