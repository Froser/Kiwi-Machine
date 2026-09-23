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

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/emulator_impl.h"
#include "nes/ppu_bus.h"
#include "third_party/nes_apu/Blip_Buffer.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k16K = 0x4000;
constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

Address GetPhysicalRegister(Byte mapper, Address logical_address) {
  if (mapper != 26)
    return logical_address;
  return static_cast<Address>((logical_address & 0xfffc) |
                              ((logical_address & 0x01) << 1) |
                              ((logical_address & 0x02) >> 1));
}

void WriteRegister(Mapper* mapper,
                   Byte mapper_id,
                   Address logical_address,
                   Byte value) {
  mapper->WritePRG(GetPhysicalRegister(mapper_id, logical_address), value);
}

void ExpectCHRBank(Mapper* mapper, size_t window, size_t bank) {
  const Address address = static_cast<Address>(window * k1K + 0x0123);
  EXPECT_EQ(mapper->ReadCHR(address), TestCHRByte(bank * k1K + 0x0123));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address), bank * k1K + 0x0123);
}

}  // namespace

class Mapper024Test : public MapperTest {};

TEST_F(Mapper024Test, CreatesBothVRC6VariantsWithProgramRAM) {
  for (Byte mapper_id : {24, 26}) {
    ASSERT_TRUE(Mapper::IsMapperSupported(mapper_id));
    auto cartridge = LoadMapper(mapper_id, 16, 32);
    ASSERT_TRUE(cartridge);
    Mapper* mapper = cartridge->mapper();

    EXPECT_TRUE(mapper->HasPRGRAM());
    EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));

    EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
    mapper->WriteExtendedRAM(0x6123, 0x5a);
    EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
    WriteRegister(mapper, mapper_id, 0xb003, 0x80);
    mapper->WriteExtendedRAM(0x6123, 0x5a);
    EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  }
}

TEST_F(Mapper024Test, MapsProgramBanksWithoutBusConflicts) {
  auto cartridge = LoadMapper(24, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8003, 0x0d);
  mapper->WritePRG(0xc003, 0x1d);

  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(13 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xb456), TestPRGByte(13 * k16K + 0x3456));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(29 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(31 * k8K + 0x0123));
}

TEST_F(Mapper024Test, SwapsA0AndA1ForMapper26) {
  auto mapper24_cartridge = LoadMapper(24, 16, 32);
  ASSERT_TRUE(mapper24_cartridge);
  Mapper* mapper24 = mapper24_cartridge->mapper();
  mapper24->WritePRG(0xd001, 0x11);
  mapper24->WritePRG(0xd002, 0x22);
  ExpectCHRBank(mapper24, 1, 0x11);
  ExpectCHRBank(mapper24, 2, 0x22);

  auto mapper26_cartridge = LoadMapper(26, 16, 32);
  ASSERT_TRUE(mapper26_cartridge);
  Mapper* mapper26 = mapper26_cartridge->mapper();
  mapper26->WritePRG(0xd001, 0x11);
  mapper26->WritePRG(0xd002, 0x22);
  ExpectCHRBank(mapper26, 1, 0x22);
  ExpectCHRBank(mapper26, 2, 0x11);
}

TEST_F(Mapper024Test, MapsCharacterWindowsInEveryBankingMode) {
  auto cartridge = LoadMapper(24, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr std::array<Byte, 8> kRegisters = {1, 3, 5, 7, 9, 11, 13, 15};
  for (size_t index = 0; index < kRegisters.size(); ++index) {
    const Address address =
        static_cast<Address>((index < 4 ? 0xd000 : 0xe000) + (index & 3));
    mapper->WritePRG(address, kRegisters[index]);
  }

  constexpr std::array<std::array<Byte, 8>, 4> kExpectedBanks = {{
      {1, 3, 5, 7, 9, 11, 13, 15},
      {0, 1, 2, 3, 4, 5, 6, 7},
      {1, 3, 5, 7, 8, 9, 10, 11},
      {1, 3, 5, 7, 8, 9, 10, 11},
  }};
  for (Byte mode = 0; mode < kExpectedBanks.size(); ++mode) {
    mapper->WritePRG(0xb003, static_cast<Byte>(0x20 | mode));
    for (size_t window = 0; window < kExpectedBanks[mode].size(); ++window)
      ExpectCHRBank(mapper, window, kExpectedBanks[mode][window]);
  }
}

TEST_F(Mapper024Test, RoutesCIRAMAndCharacterROMNametables) {
  auto cartridge = LoadMapper(24, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();
  PPUBus ppu_bus;
  ppu_bus.SetMapper(mapper);

  mapper->WritePRG(0xe000, 4);
  mapper->WritePRG(0xe001, 5);
  mapper->WritePRG(0xe002, 6);
  mapper->WritePRG(0xe003, 7);

  mapper->WritePRG(0xb003, 0x21);
  ppu_bus.Write(0x2001, 0x11);
  ppu_bus.Write(0x2401, 0x22);
  EXPECT_EQ(ppu_bus.Read(0x2801), 0x11);
  EXPECT_EQ(ppu_bus.Read(0x2c01), 0x22);

  mapper->WritePRG(0xb003, 0x31);
  EXPECT_EQ(ppu_bus.Read(0x2001), TestCHRByte(4 * k1K + 1));
  EXPECT_EQ(ppu_bus.Read(0x2401), TestCHRByte(5 * k1K + 1));
  EXPECT_EQ(ppu_bus.Read(0x2801), TestCHRByte(6 * k1K + 1));
  EXPECT_EQ(ppu_bus.Read(0x2c01), TestCHRByte(7 * k1K + 1));
}

TEST_F(Mapper024Test, SelectsAllCommercialMirroringModes) {
  auto cartridge = LoadMapper(24, 16, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr std::array<NametableMirroring, 4> kExpected = {
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
  };
  for (Byte mode = 0; mode < kExpected.size(); ++mode) {
    mapper->WritePRG(0xb003, static_cast<Byte>(0x20 | (mode << 2)));
    EXPECT_EQ(mapper->GetNametableMirroring(), kExpected[mode]);
  }
}

TEST_F(Mapper024Test, CountsCycleAndScanlineIRQsForBothWirings) {
  auto mapper24_cartridge = LoadMapper(24, 16, 32);
  ASSERT_TRUE(mapper24_cartridge);
  Mapper* mapper24 = mapper24_cartridge->mapper();

  mapper24->WritePRG(0xf000, 0xfe);
  mapper24->WritePRG(0xf001, 0x07);
  mapper24->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 0);
  mapper24->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper24->WritePRG(0xf002, 0);
  mapper24->M2CycleIRQ();
  mapper24->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);

  mapper24->WritePRG(0xf000, 0xff);
  mapper24->WritePRG(0xf001, 0x02);
  for (int cycle = 0; cycle < 113; ++cycle)
    mapper24->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);
  mapper24->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 3);

  auto mapper26_cartridge = LoadMapper(26, 16, 32);
  ASSERT_TRUE(mapper26_cartridge);
  Mapper* mapper26 = mapper26_cartridge->mapper();
  mapper26->WritePRG(0xf000, 0xff);
  mapper26->WritePRG(0xf002, 0x06);
  mapper26->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 4);
  mapper26->WritePRG(0xf001, 0);
  mapper26->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 4);
}

TEST_F(Mapper024Test, ProducesExpansionAudioForBothWirings) {
  for (Byte mapper_id : {24, 26}) {
    auto cartridge = LoadMapper(mapper_id, 16, 32);
    ASSERT_TRUE(cartridge);
    Mapper* mapper = cartridge->mapper();

    Blip_Buffer buffer;
    ASSERT_EQ(buffer.sample_rate(44100, 50), nullptr);
    buffer.clock_rate(1789773);
    mapper->SetExpansionAudioOutput(&buffer);
    mapper->SetExpansionAudioVolume(1.f);

    WriteRegister(mapper, mapper_id, 0x9000, 0x8f);
    WriteRegister(mapper, mapper_id, 0x9001, 0);
    WriteRegister(mapper, mapper_id, 0x9002, 0x80);
    mapper->EndExpansionAudioFrame(30000);
    buffer.end_frame(30000);

    std::vector<blip_sample_t> samples(buffer.samples_avail());
    ASSERT_GT(samples.size(), 0u);
    buffer.read_samples(samples.data(), samples.size());
    EXPECT_TRUE(std::any_of(samples.begin(), samples.end(),
                            [](blip_sample_t sample) { return sample != 0; }));
  }
}

TEST_F(Mapper024Test, RestoresBanksIRQAndProgramRAM) {
  auto cartridge = LoadMapper(26, 16, 32, 0x02);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteRegister(mapper, 26, 0x8000, 5);
  WriteRegister(mapper, 26, 0xc000, 7);
  WriteRegister(mapper, 26, 0xd001, 9);
  WriteRegister(mapper, 26, 0xb003, 0xa4);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  WriteRegister(mapper, 26, 0xf000, 0xff);
  WriteRegister(mapper, 26, 0xf001, 0x06);
  const Bytes state = SerializeMapper(mapper);

  WriteRegister(mapper, 26, 0x8000, 1);
  WriteRegister(mapper, 26, 0xc000, 2);
  WriteRegister(mapper, 26, 0xd001, 3);
  WriteRegister(mapper, 26, 0xb003, 0x20);
  mapper->WriteExtendedRAM(0x6123, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(5 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k8K + 0x0123));
  ExpectCHRBank(mapper, 1, 9);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper024Test, RendersAllVRC6AcceptanceROMsFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, std::pair<Byte, uint32_t>> expected_roms = {
      {"Akumajou Densetsu (Japan).nes", {24, 0xe349af38u}},
      {"Esper Dream 2 - Aratanaru Tatakai (Japan).nes", {26, 0x209b4bedu}},
      {"Mouryou Senki Madara (Japan).nes", {26, 0xe1383debu}},
  };
  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file())
      continue;
    const auto expected = expected_roms.find(entry.path().filename().string());
    if (expected == expected_roms.end())
      continue;

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    EXPECT_EQ(emulator_->GetRomData()->mapper, expected->second.first)
        << entry.path();
    EXPECT_EQ(static_cast<uint32_t>(emulator_->GetRomData()->crc),
              expected->second.second)
        << entry.path();

    emulator_->Run();
    Colors previous_pixels;
    bool frame_changed = false;
    bool final_frame_rendered = false;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      const std::set<Color> unique_colors(pixels.begin(), pixels.end());
      final_frame_rendered = unique_colors.size() > 1;
      if (!previous_pixels.empty() && pixels != previous_pixels)
        frame_changed = true;
      previous_pixels = pixels;
    }
    EXPECT_TRUE(final_frame_rendered) << entry.path();
    EXPECT_TRUE(frame_changed) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, expected_roms.size());
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
