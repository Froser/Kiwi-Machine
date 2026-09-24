// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/ppu_bus.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k8K = 0x2000;
constexpr size_t k2K = 0x0800;
constexpr size_t k1K = 0x0400;

void WriteBank(Mapper* mapper, Byte target, Byte value) {
  mapper->WritePRG(0x8000, target);
  mapper->WritePRG(0x8001, value);
}

}  // namespace

class Mapper206Test : public MapperTest {};

TEST_F(Mapper206Test, MapsNamco108PRGAndCHRWithoutMMC3Modes) {
  auto cartridge = LoadMapper(206, 8, 8);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteBank(mapper, 6, 3);
  WriteBank(mapper, 7, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xa000), TestPRGByte(4 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(14 * k8K));
  EXPECT_EQ(mapper->ReadPRG(0xe000), TestPRGByte(15 * k8K));

  const std::array<Byte, 6> banks{{2, 4, 6, 7, 8, 9}};
  for (Byte target = 0; target < banks.size(); ++target)
    WriteBank(mapper, target, banks[target]);
  const std::array<size_t, 8> expected{{2, 3, 4, 5, 6, 7, 8, 9}};
  for (size_t window = 0; window < expected.size(); ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(expected[window] * k1K + 0x123));
  }

  mapper->WritePRG(0x8000, 6);
  mapper->WritePRG(0xa001, 9);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
}

TEST_F(Mapper206Test, KeepsSubmapperOne32KPRGUnbanked) {
  auto cartridge = LoadMapper(206, 2, 4, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteBank(mapper, 6, 1);
  WriteBank(mapper, 7, 0);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(0x2123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(0x4123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(0x6123));
}

TEST_F(Mapper206Test, MapsMapper76FourTwoKilobyteCHRWindows) {
  auto cartridge = LoadMapper(76, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  for (Byte target = 2; target <= 5; ++target)
    WriteBank(mapper, target, target + 5);
  for (size_t window = 0; window < 4; ++window) {
    const Address address = static_cast<Address>(window * k2K + 0x234);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte((window + 7) * k2K + 0x234));
  }
}

TEST_F(Mapper206Test, SplitsMapper88CHRROMAcrossPatternTables) {
  auto cartridge = LoadMapper(88, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteBank(mapper, 0, 2);
  WriteBank(mapper, 2, 3);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(2 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadCHR(0x1123), TestCHRByte(0x43 * k1K + 0x123));
}

TEST_F(Mapper206Test, RoutesMapper95NametablesFromCHRBankBits) {
  auto cartridge = LoadMapper(95, 8, 4);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  ASSERT_TRUE(mapper->UsesCustomPPUMemoryMapping());
  WriteBank(mapper, 0, 0x20);
  WriteBank(mapper, 1, 0x00);
  ppu_bus.Write(0x2001, 0x5a);
  ppu_bus.Write(0x2801, 0xa5);
  EXPECT_EQ(ppu_bus.Read(0x2401), 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0xa5);
}

TEST_F(Mapper206Test, ControlsMapper154OneScreenMirroringOnEveryWrite) {
  auto cartridge = LoadMapper(154, 8, 16);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 0x40);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
  mapper->WritePRG(0xe000, 0x00);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
}

TEST_F(Mapper206Test, PersistsBanksMirroringAndCHRRAM) {
  auto cartridge = LoadMapper(154, 8, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteBank(mapper, 6, 3);
  WriteBank(mapper, 2, 5);
  mapper->WritePRG(0xe000, 0x40);
  mapper->WriteCHR(0x1123, 0x6d);
  const Bytes state = SerializeMapper(mapper);

  WriteBank(mapper, 6, 1);
  WriteBank(mapper, 2, 2);
  mapper->WritePRG(0xe000, 0);
  mapper->WriteCHR(0x1123, 0x22);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->ReadCHR(0x1123), 0x6d);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenHigher);
}

TEST_F(Mapper206Test, MapsAndPersistsFourScreenNametableRAM) {
  auto cartridge = LoadMapper(206, 8, 8, 0x08);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  const std::array<Address, 4> addresses{{0x2001, 0x2401, 0x2801, 0x2c01}};
  const std::array<Byte, 4> values{{0x11, 0x22, 0x33, 0x44}};
  for (size_t index = 0; index < addresses.size(); ++index)
    ppu_bus.Write(addresses[index], values[index]);
  const Bytes state = SerializeMapper(mapper);

  for (Address address : addresses)
    ppu_bus.Write(address, 0);
  ASSERT_TRUE(DeserializeMapper(mapper, state));

  for (size_t index = 0; index < addresses.size(); ++index)
    EXPECT_EQ(ppu_bus.Read(addresses[index]), values[index]);
  EXPECT_EQ(ppu_bus.Read(0x3001), values[0]);
}

TEST_F(Mapper206Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Devil Man (Japan).nes",
      "Digital Devil Story - Megami Tensei (Japan).nes",
      "Dragon Buster (Japan).nes",
      "Dragon Buster II - Yami no Fuuin (Japan).nes",
      "Dragon Slayer 4 - Drasle Family (Japan).nes",
      "Dragon Spirit - Aratanaru Densetsu (Japan).nes",
      "Family Boxing (Japan).nes",
      "Family Circuit (Japan).nes",
      "Family Mahjong (Japan) (Rev A).nes",
      "Family Tennis (Japan).nes",
      "Famista '89 - Kaimaku Ban!! (Japan).nes",
      "Fantasy Zone (USA) (Unl).nes",
      "Gauntlet (USA) (Unl).nes",
      "Indiana Jones and the Temple of Doom (USA) (Unl).nes",
      "Jikuu Yuuden - Debias (Japan).nes",
      "Lasa-r Ishii no Childs Quest (Japan).nes",
      "Lupin Sansei - Pandora no Isan (Japan).nes",
      "Namcot Mahjong III - Mahjong Tengoku (Japan).nes",
      "Quest of Ki, The (Japan).nes",
      "Quinty (Japan).nes",
      "R.B.I. Baseball (USA) (Unl).nes",
      "Ring King (USA).nes",
      "Sky Kid (Japan).nes",
      "Tenkaichi Bushi - Keru Naguuru (Japan).nes",
      "Vindicators (USA) (Unl).nes",
      "Wagan Land (Japan).nes",
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
    const Byte mapper = emulator_->GetRomData()->mapper;
    ASSERT_TRUE(mapper == 76 || mapper == 88 || mapper == 95 || mapper == 154 ||
                mapper == 206)
        << entry.path();

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
