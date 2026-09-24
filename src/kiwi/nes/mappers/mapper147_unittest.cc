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

void LoadRegister(Mapper* mapper, Byte value) {
  mapper->WriteExtendedRAM(0x4102, value);
  mapper->WriteExtendedRAM(0x4100, 0);
}

}  // namespace

class Mapper147Test : public MapperTest {};

TEST_F(Mapper147Test, StartsWithSupportedBankZeroAndHeaderMirroring) {
  ASSERT_TRUE(Mapper::IsMapperSupported(147));
  auto cartridge = LoadMapper(147, 8, 16, 0x01);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456), 0x0456u);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
}

TEST_F(Mapper147Test, LatchesSplitProgramBitsAndCharacterBank) {
  auto cartridge = LoadMapper(147, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  LoadRegister(mapper, 0xd4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));

  mapper->WritePRG(0x8000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(10 * k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456),
            static_cast<uint32_t>(10 * k8K + 0x0456));
}

TEST_F(Mapper147Test, ImplementsJv001InvertIncrementAndReadback) {
  auto cartridge = LoadMapper(147, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4101, 0x04);
  LoadRegister(mapper, 0xac);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x51);

  mapper->WritePRG(0xffff, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * k8K + 0x0456));

  mapper->WriteExtendedRAM(0x4103, 0x04);
  mapper->WriteExtendedRAM(0x4100, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x55);
  mapper->WritePRG(0x8000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(2 * k8K + 0x0456));
}

TEST_F(Mapper147Test, DecodesRegisterMirrorsAndIgnoresOtherAddresses) {
  auto cartridge = LoadMapper(147, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4202, 0xfc);
  mapper->WriteExtendedRAM(0x4200, 0);
  mapper->WritePRG(0x8000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4200), 0x42);

  mapper->WriteExtendedRAM(0x5102, 0xd4);
  mapper->WriteExtendedRAM(0x5100, 0);
  mapper->WritePRG(0x8000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(10 * k8K + 0x0456));
}

TEST_F(Mapper147Test, RestoresChipAndOutputStateThenResets) {
  auto cartridge = LoadMapper(147, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  LoadRegister(mapper, 0xd4);
  mapper->WriteExtendedRAM(0x4103, 0x04);
  mapper->WritePRG(0x8000, 0);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x4100, 0);
  mapper->WritePRG(0x8000, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(2 * k32K + 0x0123));

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(10 * k8K + 0x0456));
  mapper->WriteExtendedRAM(0x4100, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0xd9);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x01);
}

TEST_F(Mapper147Test, RendersChineseKungFuFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Chinese KungFu (Asia) (Unl).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 147) << entry.path();

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
