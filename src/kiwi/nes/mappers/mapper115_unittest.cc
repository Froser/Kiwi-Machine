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

#include "nes/mappers/mapper115.h"

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/emulator_impl.h"
#include "nes/mappers/mapper_test_support.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k1K = 0x0400;
constexpr size_t k8K = 0x2000;

Byte ExpectedPRGByte(size_t bank, Address address) {
  return TestPRGByte(bank * k8K + (address & (k8K - 1)));
}

}  // namespace

class Mapper115Test : public MapperTest {};

TEST_F(Mapper115Test, CreatesMMC3BoardWithPowerOnOuterBanks) {
  ASSERT_TRUE(Mapper::IsMapperSupported(115));
  auto cartridge = LoadMapper(115, 32, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 3);
  WriteMMC3Register(mapper, 7, 4);
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(3, 0x8123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), ExpectedPRGByte(4, 0xa123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), ExpectedPRGByte(30, 0xc123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), ExpectedPRGByte(31, 0xe123));

  mapper->WriteExtendedRAM(0x6000, 0x40);
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(35, 0x8123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), ExpectedPRGByte(36, 0xa123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), ExpectedPRGByte(62, 0xc123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), ExpectedPRGByte(63, 0xe123));

  WriteMMC3Register(mapper, 6, 5, 0x40);
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(62, 0x8123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), ExpectedPRGByte(36, 0xa123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), ExpectedPRGByte(37, 0xc123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), ExpectedPRGByte(63, 0xe123));
}

TEST_F(Mapper115Test, OverridesMMC3WithNROM128AndNROM256Banks) {
  auto cartridge = LoadMapper(115, 32, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 6, 7);
  WriteMMC3Register(mapper, 7, 8);

  mapper->WriteExtendedRAM(0x6000, 0xcb);
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(54, 0x8123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), ExpectedPRGByte(55, 0xa123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), ExpectedPRGByte(54, 0xc123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), ExpectedPRGByte(55, 0xe123));

  mapper->WriteExtendedRAM(0x6000, 0xfb);
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(52, 0x8123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), ExpectedPRGByte(53, 0xa123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), ExpectedPRGByte(54, 0xc123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), ExpectedPRGByte(55, 0xe123));

  mapper->WriteExtendedRAM(0x6000, 0x40);
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(39, 0x8123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), ExpectedPRGByte(40, 0xa123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), ExpectedPRGByte(62, 0xc123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), ExpectedPRGByte(63, 0xe123));
}

TEST_F(Mapper115Test, SelectsOuterCharacterBankAndAbsoluteAddress) {
  auto cartridge = LoadMapper(115, 32, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 2, 0x12);
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(0x12 * k1K + 0x155));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1155), 0x12u * k1K + 0x155);

  mapper->WritePRG(0xa001, 0x00);
  mapper->WriteExtendedRAM(0x7ffd, 0xff);
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(0x112 * k1K + 0x155));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x1155), 0x112u * k1K + 0x155);

  WriteMMC3Register(mapper, 2, 0x13, 0x80);
  EXPECT_EQ(mapper->ReadCHR(0x0155), TestCHRByte(0x113 * k1K + 0x155));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0155), 0x113u * k1K + 0x155);
}

TEST_F(Mapper115Test, DecodesLowRegistersWithE003Mask) {
  auto cartridge = LoadMapper(115, 32, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6003, 0x5a);
  mapper->WriteExtendedRAM(0x7fff, 0xa5);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6003), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7fff), 0xa5);

  mapper->WriteExtendedRAM(0x5000, 0xcb);
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(0, 0x8123));

  mapper->WritePRG(0xa001, 0x00);
  mapper->WriteExtendedRAM(0x7ffc, 0xcb);
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(54, 0x8123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), ExpectedPRGByte(54, 0xc123));

  mapper->WriteExtendedRAM(0x6002, 0xff);
  mapper->WriteExtendedRAM(0x7ffe, 0xff);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6002), 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7ffe), 0);
}

TEST_F(Mapper115Test, RetainsMMC3MirroringAndSharpZeroLatchIRQ) {
  auto cartridge = LoadMapper(115, 32, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa000, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WritePRG(0xa000, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);

  mapper->WritePRG(0xc000, 0);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 2);
}

TEST_F(Mapper115Test, PersistsOuterAndMMC3StateAndResetsOuterRegisters) {
  auto cartridge = LoadMapper(115, 32, 64);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC3Register(mapper, 2, 0x12);
  WriteMMC3Register(mapper, 6, 3);
  mapper->WriteExtendedRAM(0x6000, 0x40);
  mapper->WriteExtendedRAM(0x6001, 1);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xc000, 1);
  mapper->WritePRG(0xc001, 0);
  mapper->WritePRG(0xe001, 0);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x6000, 0x80);
  mapper->WriteExtendedRAM(0x6001, 0);
  WriteMMC3Register(mapper, 6, 7);
  mapper->WritePRG(0xa000, 0);
  mapper->WritePRG(0xe000, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(35, 0x8123));
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(0x112 * k1K + 0x155));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  mapper->ScanlineIRQ(0, true);
  mapper->ScanlineIRQ(1, true);
  EXPECT_EQ(irq_count_, 1);

  mapper->Reset();
  EXPECT_EQ(mapper->ReadPRG(0x8123), ExpectedPRGByte(3, 0x8123));
  EXPECT_EQ(mapper->ReadCHR(0x1155), TestCHRByte(0x12 * k1K + 0x155));
}

TEST_F(Mapper115Test, RendersAVKyuukyokuMahjong2For600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::string expected_rom = "AV Kyuukyoku Mahjong 2 (Asia) (Unl).nes";
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
    EXPECT_EQ(emulator_->GetRomData()->crc, 0x77da06cfu);
    EXPECT_EQ(emulator_->GetRomData()->mapper, 115);

    emulator_->Run();
    Colors previous_pixels;
    bool rendered = false;
    bool frame_changed = false;
    size_t final_unique_colors = 0;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      final_unique_colors =
          std::set<Color>(pixels.begin(), pixels.end()).size();
      rendered = rendered || final_unique_colors > 1;
      if (!previous_pixels.empty() && pixels != previous_pixels)
        frame_changed = true;
      previous_pixels = pixels;
    }

    EXPECT_TRUE(rendered) << entry.path();
    EXPECT_TRUE(frame_changed) << entry.path();
    EXPECT_GT(final_unique_colors, 1u) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, 1u);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
