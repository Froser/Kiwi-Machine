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

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

}  // namespace

class Mapper022Test : public MapperTest {};

TEST_F(Mapper022Test, MapsVRC2aProgramAndShiftedCharacterBanks) {
  ASSERT_TRUE(Mapper::IsMapperSupported(22));
  auto cartridge = LoadMapper(22, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8003, 3);
  mapper->WritePRG(0xa002, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(14 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(15 * k8K + 0x0123));

  constexpr std::array<Address, 8> kLowRegisters{{
      0xb000,
      0xb001,
      0xc000,
      0xc001,
      0xd000,
      0xd001,
      0xe000,
      0xe001,
  }};
  constexpr std::array<Address, 8> kHighRegisters{{
      0xb002,
      0xb003,
      0xc002,
      0xc003,
      0xd002,
      0xd003,
      0xe002,
      0xe003,
  }};
  for (size_t window = 0; window < kLowRegisters.size(); ++window) {
    const Byte raw_bank = static_cast<Byte>((window + 3) * 2);
    mapper->WritePRG(kLowRegisters[window], raw_bank & 0x0f);
    mapper->WritePRG(kHighRegisters[window], raw_bank >> 4);
  }

  for (size_t window = 0; window < kLowRegisters.size(); ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    const size_t expected_bank = window + 3;
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(expected_bank * k1K + 0x123));
    EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address),
              expected_bank * k1K + 0x123);
  }

  mapper->WritePRG(0xb000, 0x07);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(3 * k1K + 0x123));
}

TEST_F(Mapper022Test, UsesVRC2MirroringAndGroundedMicrowireInput) {
  auto cartridge = LoadMapper(22, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_FALSE(mapper->NeedsM2CycleIRQ());
  mapper->WritePRG(0x9000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0x9002, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0x9000, 3);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mirroring_changes_, 2);

  mapper->WriteExtendedRAM(0x6123, 1);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x60);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7000), 0x70);
}

TEST_F(Mapper022Test, PersistsBanksMirroringAndCHRRAM) {
  auto cartridge = LoadMapper(22, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0xb000, 6);
  mapper->WritePRG(0x9000, 0);
  mapper->WriteCHR(0x0123, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0xb000, 0);
  mapper->WritePRG(0x9000, 1);
  mapper->WriteCHR(0x0123, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper022Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Ganbare Pennant Race! (Japan).nes",
      "TwinBee 3 - Poko Poko Dai Maou (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 22) << entry.path();

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
