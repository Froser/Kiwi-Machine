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

constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

}  // namespace

class Mapper021Test : public MapperTest {};

TEST_F(Mapper021Test, MapsExactVRC4aAndVRC4cWirings) {
  ASSERT_TRUE(Mapper::IsMapperSupported(21));

  auto vrc4a = LoadMapper(21, 8, 64, 0, 1);
  ASSERT_TRUE(vrc4a);
  Mapper* mapper = vrc4a->mapper();
  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0xa000, 4);
  mapper->WritePRG(0x9004, 2);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(15 * k8K));
  mapper->WritePRG(0xb000, 0x05);
  mapper->WritePRG(0xb002, 0x01);
  mapper->WritePRG(0xb004, 0x06);
  mapper->WritePRG(0xb006, 0x02);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x15 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadCHR(0x0523), TestCHRByte(0x26 * k1K + 0x123));

  auto vrc4c = LoadMapper(21, 8, 64, 0, 2);
  ASSERT_TRUE(vrc4c);
  mapper = vrc4c->mapper();
  mapper->WritePRG(0xb000, 0x07);
  mapper->WritePRG(0xb040, 0x03);
  mapper->WritePRG(0xb080, 0x08);
  mapper->WritePRG(0xb0c0, 0x04);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x37 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadCHR(0x0523), TestCHRByte(0x48 * k1K + 0x123));
}

TEST_F(Mapper021Test, AcceptsBothLegacyVRC4AddressWirings) {
  auto cartridge = LoadMapper(21, 8, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xb000, 0x05);
  mapper->WritePRG(0xb002, 0x01);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x15 * k1K + 0x123));

  mapper->WritePRG(0xb000, 0x06);
  mapper->WritePRG(0xb040, 0x02);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(0x26 * k1K + 0x123));

  mapper->WritePRG(0xb004, 0x07);
  mapper->WritePRG(0xb006, 0x03);
  EXPECT_EQ(mapper->ReadCHR(0x0523), TestCHRByte(0x37 * k1K + 0x123));

  mapper->WritePRG(0xb080, 0x08);
  mapper->WritePRG(0xb0c0, 0x04);
  EXPECT_EQ(mapper->ReadCHR(0x0523), TestCHRByte(0x48 * k1K + 0x123));
}

TEST_F(Mapper021Test, SupportsMirroringIRQAndStateRoundTrip) {
  auto cartridge = LoadMapper(21, 8, 0, 0, 1, 0, 0x05);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0x9000, 2);
  mapper->WritePRG(0xf000, 0x0e);
  mapper->WritePRG(0xf002, 0x0f);
  mapper->WritePRG(0xf004, 0x06);
  mapper->M2CycleIRQ();
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  mapper->WriteCHR(0x0123, 0x45);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0x9000, 0);
  mapper->WritePRG(0xf006, 0);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
  mapper->WriteCHR(0x0123, 0x67);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  EXPECT_TRUE(mapper->NeedsM2CycleIRQ());
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper021Test, UsesVRC4cIRQAddressWiring) {
  auto cartridge = LoadMapper(21, 8, 16, 0, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xf000, 0x0e);
  mapper->WritePRG(0xf040, 0x0f);
  mapper->WritePRG(0xf080, 0x06);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->WritePRG(0xf0c0, 0);
  for (int cycle = 0; cycle < 4; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper021Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Ganbare Goemon Gaiden 2 - Tenka no Zaihou (Japan).nes",
      "Wai Wai World 2 - SOS!! Paseri Jou (Japan).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 21) << entry.path();

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
