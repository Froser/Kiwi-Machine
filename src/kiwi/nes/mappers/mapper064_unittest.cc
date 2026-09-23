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

void WriteBank(Mapper* mapper, Byte target, Byte value, Byte mode = 0) {
  mapper->WritePRG(0x8000, static_cast<Byte>(target | mode));
  mapper->WritePRG(0x8001, value);
}

}  // namespace

class Mapper064Test : public MapperTest {};

TEST_F(Mapper064Test, CreatesRambo1AndMapsEveryPRGWindow) {
  ASSERT_TRUE(Mapper::IsMapperSupported(64));
  auto cartridge = LoadMapper(64, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteBank(mapper, 6, 3);
  WriteBank(mapper, 7, 4);
  WriteBank(mapper, 15, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(5 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(15 * k8K + 0x0123));

  mapper->WritePRG(0x8000, 0x46);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(3 * k8K + 0x0123));
}

TEST_F(Mapper064Test, MapsCHRInTwoKilobyteAndOneKilobyteModes) {
  auto cartridge = LoadMapper(64, 8, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const std::array<Byte, 8> registers{{2, 4, 6, 7, 8, 9, 10, 11}};
  const std::array<Byte, 8> targets{{0, 1, 2, 3, 4, 5, 8, 9}};
  for (size_t i = 0; i < targets.size(); ++i)
    WriteBank(mapper, targets[i], registers[i]);

  const std::array<size_t, 8> two_k_banks{{2, 3, 4, 5, 6, 7, 8, 9}};
  for (size_t page = 0; page < two_k_banks.size(); ++page) {
    const Address address = static_cast<Address>(page * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(two_k_banks[page] * k1K + 0x123));
  }

  mapper->WritePRG(0x8000, 0x20);
  const std::array<size_t, 8> one_k_banks{{2, 10, 4, 11, 6, 7, 8, 9}};
  for (size_t page = 0; page < one_k_banks.size(); ++page) {
    const Address address = static_cast<Address>(page * k1K + 0x234);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(one_k_banks[page] * k1K + 0x234));
  }

  mapper->WritePRG(0x8000, 0xa0);
  const std::array<size_t, 8> inverted_banks{{6, 7, 8, 9, 2, 10, 4, 11}};
  for (size_t page = 0; page < inverted_banks.size(); ++page) {
    const Address address = static_cast<Address>(page * k1K + 0x345);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(inverted_banks[page] * k1K + 0x345));
  }
}

TEST_F(Mapper064Test, SelectsHorizontalAndVerticalMirroring) {
  auto cartridge = LoadMapper(64, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->WritePRG(0xa000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mirroring_changes_, 2);
}

TEST_F(Mapper064Test, TriggersCycleIRQAfterFourClocksAndOneCycleDelay) {
  auto cartridge = LoadMapper(64, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());
  mapper->WritePRG(0xc000, 0);
  mapper->WritePRG(0xc001, 1);
  mapper->WritePRG(0xe001, 0);
  for (int cycle = 0; cycle < 4; ++cycle) {
    mapper->M2CycleIRQ();
    EXPECT_EQ(irq_count_, 0);
  }
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0xe000, 0);
  for (int cycle = 0; cycle < 8; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper064Test, TriggersScanlineIRQAfterTwoCycleDelay) {
  auto cartridge = LoadMapper(64, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xc000, 0);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  mapper->ScanlineIRQ(0, true);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WritePRG(0xe000, 0);
  mapper->ScanlineIRQ(1, true);
  mapper->M2CycleIRQ();
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper064Test, PersistsBankIRQMirroringAndCHRRAMState) {
  auto cartridge = LoadMapper(64, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteBank(mapper, 6, 3);
  mapper->WritePRG(0xa000, 0);
  mapper->WritePRG(0xc000, 0);
  mapper->WritePRG(0xc001, 1);
  mapper->WritePRG(0xe001, 0);
  for (int cycle = 0; cycle < 4; ++cycle)
    mapper->M2CycleIRQ();
  mapper->WriteCHR(0x0123, 0x5a);
  const Bytes state = SerializeMapper(mapper);

  WriteBank(mapper, 6, 1);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xe000, 0);
  mapper->WriteCHR(0x0123, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper064Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Klax (USA) (Unl).nes",
      "Road Runner (USA) (Unl).nes",
      "Rolling Thunder (USA) (Unl).nes",
      "Shinobi (USA) (Unl).nes",
      "Skull & Crossbones (USA) (Unl).nes",
      "Xybots (USA) (Unl) (Proto).nes",
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
    ASSERT_EQ(emulator_->GetRomData()->mapper, 64) << entry.path();

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
