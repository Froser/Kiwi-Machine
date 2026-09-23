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

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k32K = 0x8000;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper148Test : public MapperTest {};

TEST_F(Mapper148Test, StartsWithSupportedFixedBankZeroMapping) {
  ASSERT_TRUE(Mapper::IsMapperSupported(148));
  auto cartridge = LoadMapper(148, 4, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xffff), TestPRGByte(0x7fff));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
}

TEST_F(Mapper148Test,
       AppliesBusConflictsBeforeSelectingProgramAndCharacterBanks) {
  auto cartridge = LoadMapper(148, 4, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  // Bank-zero ROM at $800D is $0D, so $FF resolves to PCCC=$0D.
  mapper->WritePRG(0x800d, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(5 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 5 * k8K + 0x0456);

  // Bank-one ROM at $8000 is $A0, suppressing all PCCC bits from $0F.
  mapper->WritePRG(0x8000, 0x0f);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
}

TEST_F(Mapper148Test, KeepsCharacterRomReadOnlyAndHeaderMirroring) {
  auto cartridge = LoadMapper(148, 4, 8, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Byte original = mapper->ReadCHR(0x0789);
  mapper->WriteCHR(0x0789, original ^ 0xff);

  EXPECT_EQ(mapper->ReadCHR(0x0789), original);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mirroring_changes_, 0);
}

TEST_F(Mapper148Test, RestoresSelectedBanksAndResetReturnsToBankZero) {
  auto cartridge = LoadMapper(148, 4, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x800d, 0xff);
  const Bytes state = SerializeMapper(mapper);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0));

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(k32K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(5 * k8K));

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0));
}

TEST_F(Mapper148Test, RendersMahjongTrapFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom =
      "Mahjong Trap - Si Cuan Ma Que - Zhi Fu Pian (Asia) (Unl).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 148) << entry.path();

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
