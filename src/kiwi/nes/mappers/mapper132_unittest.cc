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

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k32K = 0x8000;
constexpr size_t k8K = 0x2000;

}  // namespace

class Mapper132Test : public MapperTest {};

TEST_F(Mapper132Test, LatchesProtectionStateIntoPRGAndCHRBanks) {
  ASSERT_TRUE(Mapper::IsMapperSupported(132));
  auto cartridge = LoadMapper(132, 4, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));

  mapper->WriteExtendedRAM(0x4102, 0x05);
  mapper->WriteExtendedRAM(0x4100, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x45);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(0x0456));

  mapper->WritePRG(0x8000, 0xff);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(k32K + 0x0123));
  EXPECT_EQ(mapper->ReadCHR(0x0456), TestCHRByte(k8K + 0x0456));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0456),
            static_cast<uint32_t>(k8K + 0x0456));
}

TEST_F(Mapper132Test, ImplementsStagingIncrementInvertAndRegisterMirrors) {
  auto cartridge = LoadMapper(132, 4, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x5102, 0x0b);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x5f00), 0x58);

  mapper->WriteExtendedRAM(0x4300, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x4b);

  mapper->WriteExtendedRAM(0x4103, 1);
  mapper->WriteExtendedRAM(0x4100, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x4c);

  mapper->WriteExtendedRAM(0x4101, 1);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x44);

  mapper->WriteExtendedRAM(0x4103, 0);
  mapper->WriteExtendedRAM(0x4102, 2);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x4c);
  mapper->WriteExtendedRAM(0x4100, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x4d);
}

TEST_F(Mapper132Test, RestoresStateAndResetReturnsToPowerOnMapping) {
  auto cartridge = LoadMapper(132, 4, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x4102, 5);
  mapper->WriteExtendedRAM(0x4100, 0);
  mapper->WriteExtendedRAM(0x4103, 1);
  mapper->WritePRG(0x8000, 0);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x4100, 0);
  mapper->WritePRG(0x8000, 0);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k8K));

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x45);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(k32K));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(k8K));

  mapper->WriteExtendedRAM(0x4100, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x46);
  mapper->WritePRG(0xffff, 0);
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(2 * k8K));

  mapper->Reset();
  EXPECT_EQ(mapper->ReadExtendedRAM(0x4100), 0x40);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));
  EXPECT_EQ(mapper->ReadCHR(0), TestCHRByte(0));
}

TEST_F(Mapper132Test, RendersCreatom) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "Creatom (Unl).nes";
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 132) << entry.path();

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
