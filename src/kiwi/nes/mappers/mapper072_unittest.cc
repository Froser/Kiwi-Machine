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

void SetPRGByte(Bytes* rom, size_t bank, Address address, Byte value) {
  ASSERT_TRUE(rom);
  const size_t index = kINESHeaderSize + bank * k16K + (address & 0x3fff);
  ASSERT_LT(index, rom->size());
  (*rom)[index] = value;
}

Bytes MakeWritableRegisterROM() {
  Bytes rom = MakeTestROM(72, 8, 16, 0x01);
  for (size_t bank = 0; bank < 8; ++bank)
    SetPRGByte(&rom, bank, 0x80ff, 0xff);
  return rom;
}

}  // namespace

class Mapper072Test : public MapperTest {
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

TEST_F(Mapper072Test, CreatesJalecoJF17WithResetLayout) {
  ASSERT_TRUE(Mapper::IsMapperSupported(72));
  auto cartridge = LoadMapper(72, 8, 16, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper072Test, AppliesPRGROMBusConflictsBeforeLatchingBanks) {
  Bytes rom = MakeTestROM(72, 8, 16);
  SetPRGByte(&rom, 0, 0x80c1, 0xc1);
  auto cartridge = LoadCustomROM(rom);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80c1, 0xcf);

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(k8K + 0x0456));
}

TEST_F(Mapper072Test, LatchesPRGAndCHRSelectionOnlyOnRisingEdges) {
  auto cartridge = LoadCustomROM(MakeWritableRegisterROM());
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0x83);
  mapper->WritePRG(0x80ff, 0x85);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k16K + 0x0123));

  mapper->WritePRG(0x80ff, 0x00);
  mapper->WritePRG(0x80ff, 0x85);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));

  mapper->WritePRG(0x80ff, 0x42);
  mapper->WritePRG(0x80ff, 0x45);
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * k8K + 0x0456));

  mapper->WritePRG(0x80ff, 0x00);
  mapper->WritePRG(0x80ff, 0x45);
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(5 * k8K + 0x0456));
}

TEST_F(Mapper072Test, ResetsAndRestoresBanksAndEdgeLatchState) {
  auto cartridge = LoadCustomROM(MakeWritableRegisterROM());
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x80ff, 0xc5);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x80ff, 0x00);
  mapper->WritePRG(0x80ff, 0xc1);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(k8K + 0x0456));

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  mapper->WritePRG(0x80ff, 0xc2);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(5 * k8K + 0x0456));

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));

  mapper->WritePRG(0x80ff, 0xc2);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * k8K + 0x0456));
}

TEST_F(Mapper072Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Moero!! Juudou Warriors (Japan).nes",
      "Moero!! Pro Tennis (Japan).nes",
      "Pinball Quest (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 72) << entry.path();

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
