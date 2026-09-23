// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <cstdlib>
#include <filesystem>
#include <iterator>
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
constexpr size_t k1K = 0x0400;

}  // namespace

class Mapper080Test : public MapperTest {};

TEST_F(Mapper080Test, MapsX1005PRGAndCHRRegisters) {
  auto cartridge = LoadMapper(80, 32, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x7efa, 3);
  mapper->WriteExtendedRAM(0x7efc, 4);
  mapper->WriteExtendedRAM(0x7efe, 5);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(5 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(63 * k8K + 0x0123));

  mapper->WriteExtendedRAM(0x7ef0, 3);
  mapper->WriteExtendedRAM(0x7ef1, 4);
  mapper->WriteExtendedRAM(0x7ef2, 6);
  mapper->WriteExtendedRAM(0x7ef3, 7);
  mapper->WriteExtendedRAM(0x7ef4, 8);
  mapper->WriteExtendedRAM(0x7ef5, 9);
  const size_t expected_banks[] = {2, 3, 4, 5, 6, 7, 8, 9};
  for (size_t window = 0; window < std::size(expected_banks); ++window) {
    const Address address = static_cast<Address>(window * k1K + 0x123);
    EXPECT_EQ(mapper->ReadCHR(address),
              TestCHRByte(expected_banks[window] * k1K + 0x123));
  }
}

TEST_F(Mapper080Test, MirrorsX1005RegistersAndProtectsInternalRAM) {
  auto cartridge = LoadMapper(80, 8, 16, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x7e7a, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  mapper->WriteExtendedRAM(0x7ef6, 0);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  mapper->WriteExtendedRAM(0x7ef7, 1);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);

  mapper->WriteExtendedRAM(0x7f00, 0x11);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7f00), 0x7f);
  mapper->WriteExtendedRAM(0x7ef8, 0xa3);
  mapper->WriteExtendedRAM(0x7f00, 0x22);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7f80), 0x22);
  mapper->WriteExtendedRAM(0x7f80, 0x33);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7f00), 0x33);

  const auto snapshot = mapper->ExportPRGNVRAM(false);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->data.size(), 0x80u);
  EXPECT_EQ(snapshot->data[0], 0x33);
}

TEST_F(Mapper080Test, RoutesMapper207NametablesFromCHRBankBits) {
  auto cartridge = LoadMapper(207, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  mapper->WriteExtendedRAM(0x7ef0, 0x82);
  mapper->WriteExtendedRAM(0x7ef1, 0x04);
  ppu_bus.Write(0x2001, 0x5a);
  ppu_bus.Write(0x2801, 0xa5);

  EXPECT_EQ(ppu_bus.Read(0x2401), 0x5a);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0xa5);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(2 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadCHR(0x0923), TestCHRByte(4 * k1K + 0x123));
}

TEST_F(Mapper080Test, MapsX1017CHRModeAndReorderedPRGBits) {
  auto cartridge = LoadMapper(82, 8, 32, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x7ef0, 2);
  mapper->WriteExtendedRAM(0x7ef1, 4);
  mapper->WriteExtendedRAM(0x7ef2, 6);
  mapper->WriteExtendedRAM(0x7ef3, 7);
  mapper->WriteExtendedRAM(0x7ef4, 8);
  mapper->WriteExtendedRAM(0x7ef5, 9);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(2 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadCHR(0x1123), TestCHRByte(6 * k1K + 0x123));

  mapper->WriteExtendedRAM(0x7ef6, 0x03);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(6 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadCHR(0x1123), TestCHRByte(2 * k1K + 0x123));

  mapper->WriteExtendedRAM(0x7efa, 3 << 2);
  mapper->WriteExtendedRAM(0x7efb, 4 << 2);
  mapper->WriteExtendedRAM(0x7efc, 5 << 2);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(5 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(15 * k8K + 0x0123));
}

TEST_F(Mapper080Test, ProtectsX1017RAMRegionsAndPersistsNVRAM) {
  auto cartridge = LoadMapper(82, 8, 32, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x6001, 0x11);
  mapper->WriteExtendedRAM(0x6801, 0x22);
  mapper->WriteExtendedRAM(0x7001, 0x33);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6801), 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7001), 0);

  mapper->WriteExtendedRAM(0x7ef7, 0xca);
  mapper->WriteExtendedRAM(0x7ef8, 0x69);
  mapper->WriteExtendedRAM(0x7ef9, 0x84);
  mapper->WriteExtendedRAM(0x6001, 0x44);
  mapper->WriteExtendedRAM(0x6801, 0x55);
  mapper->WriteExtendedRAM(0x7001, 0x66);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x44);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6801), 0x55);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x7001), 0x66);

  const auto snapshot = mapper->ExportPRGNVRAM(false);
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->data.size(), 0x1400u);
  EXPECT_EQ(snapshot->data[0x0001], 0x44);
  EXPECT_EQ(snapshot->data[0x0801], 0x55);
  EXPECT_EQ(snapshot->data[0x1001], 0x66);

  mapper->WriteExtendedRAM(0x7ef8, 0);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6801), 0);
}

TEST_F(Mapper080Test, CountsX1017M2CycleIRQAndReloadsOnAcknowledge) {
  auto cartridge = LoadMapper(82, 8, 32, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  ASSERT_TRUE(mapper->NeedsM2CycleIRQ());
  mapper->WriteExtendedRAM(0x7efd, 1);
  mapper->WriteExtendedRAM(0x7efe, 0);
  mapper->WriteExtendedRAM(0x7efe, 0x03);
  for (int cycle = 0; cycle < 47; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  mapper->WriteExtendedRAM(0x7eff, 0);
  for (int cycle = 0; cycle < 31; ++cycle)
    mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);
}

TEST_F(Mapper080Test, PersistsX1017BankProtectionAndIRQState) {
  auto cartridge = LoadMapper(82, 8, 32, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x7ef2, 9);
  mapper->WriteExtendedRAM(0x7ef6, 0x03);
  mapper->WriteExtendedRAM(0x7efa, 3 << 2);
  mapper->WriteExtendedRAM(0x7ef7, 0xca);
  mapper->WriteExtendedRAM(0x6001, 0x5a);
  mapper->WriteExtendedRAM(0x7efd, 2);
  mapper->WriteExtendedRAM(0x7efe, 0x03);
  const Bytes state = SerializeMapper(mapper);

  mapper->WriteExtendedRAM(0x7ef2, 1);
  mapper->WriteExtendedRAM(0x7ef6, 0);
  mapper->WriteExtendedRAM(0x7efa, 0);
  mapper->WriteExtendedRAM(0x7ef7, 0);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadCHR(0x0123), TestCHRByte(9 * k1K + 0x123));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(3 * k8K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kVertical);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6001), 0x5a);
}

TEST_F(Mapper080Test, BanksX1017CHRRAM) {
  auto cartridge = LoadMapper(82, 8, 0, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteExtendedRAM(0x7ef6, 0x02);
  mapper->WriteExtendedRAM(0x7ef2, 1);
  mapper->WriteCHR(0x0123, 0x6d);
  mapper->WriteExtendedRAM(0x7ef2, 0);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0);
  mapper->WriteExtendedRAM(0x7ef2, 1);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x6d);
}

TEST_F(Mapper080Test, RendersAllAcceptanceROMs) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::set<std::string> expected_roms = {
      "Fudou Myouou Den (Japan).nes",
      "Kyonsees 2 (Japan).nes",
      "Kyuukyoku Harikiri Koushien (Japan).nes",
      "Kyuukyoku Harikiri Stadium '88 (Japan) (Rev 1).nes",
      "Kyuukyoku Harikiri Stadium '88 (Japan).nes",
      "Kyuukyoku Harikiri Stadium - Heisei Gannen Ban (Japan).nes",
      "Kyuukyoku Harikiri Stadium 3 (Japan).nes",
      "Minelvaton Saga (Japan).nes",
      "Mirai Shinwa Jarvas (Japan).nes",
      "SD Keiji - Blader (Japan).nes",
      "Taito Grand Prix - Eikou heno License (Japan).nes",
      "Yamamura Misa Suspense - Kyouto Ryuu no Tera Satsujin Jiken (Japan).nes",
  };

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    const std::string filename = entry.path().filename().string();
    if (!entry.is_regular_file() || !expected_roms.contains(filename))
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    if (filename == "Fudou Myouou Den (Japan).nes") {
      ASSERT_EQ(emulator_->GetRomData()->mapper, 207) << entry.path();
    } else {
      const Byte mapper = emulator_->GetRomData()->mapper;
      ASSERT_TRUE(mapper == 80 || mapper == 82) << entry.path();
    }

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
