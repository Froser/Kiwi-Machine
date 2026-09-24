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
#include <map>
#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/emulator_impl.h"
#include "third_party/nes_apu/Blip_Buffer.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k8K = 0x2000;
constexpr size_t k1K = 0x0400;

Address RegisterAddress(Byte submapper, Address vrc7a_address) {
  if (submapper != 1 || (vrc7a_address & 0xf030) == 0x9010 ||
      (vrc7a_address & 0xf030) == 0x9030) {
    return vrc7a_address;
  }
  return static_cast<Address>((vrc7a_address & ~0x0018) |
                              ((vrc7a_address & 0x0010) >> 1));
}

void WriteRegister(Mapper* mapper,
                   Byte submapper,
                   Address vrc7a_address,
                   Byte value) {
  mapper->WritePRG(RegisterAddress(submapper, vrc7a_address), value);
}

void AdvanceCycles(Mapper* mapper, int cycles) {
  for (int cycle = 0; cycle < cycles; ++cycle)
    mapper->M2CycleIRQ();
}

void WriteAudioRegister(Mapper* mapper, Byte reg, Byte value) {
  mapper->WritePRG(0x9010, reg);
  AdvanceCycles(mapper, 6);
  mapper->WritePRG(0x9030, value);
  AdvanceCycles(mapper, 42);
}

bool HasNonzeroSamples(Blip_Buffer* buffer) {
  std::vector<blip_sample_t> samples(buffer->samples_avail());
  if (samples.empty())
    return false;
  buffer->read_samples(samples.data(), samples.size());
  return std::any_of(samples.begin(), samples.end(),
                     [](blip_sample_t sample) { return sample != 0; });
}

}  // namespace

class Mapper085Test : public MapperTest {};

TEST_F(Mapper085Test, CreatesVRC7WithFixedLastBankAndProtectedProgramRAM) {
  ASSERT_TRUE(Mapper::IsMapperSupported(85));
  auto cartridge = LoadMapper(85, 32, 32, 0, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_TRUE(mapper->HasPRGRAM());
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(63 * k8K + 0x0123));

  mapper->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
  mapper->WritePRG(0xe000, 0x80);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
}

TEST_F(Mapper085Test, MapsAllProgramAndCharacterWindowsForBothWirings) {
  for (Byte submapper : {1, 2}) {
    auto cartridge = LoadMapper(85, 32, 32, 0, submapper);
    ASSERT_TRUE(cartridge);
    Mapper* mapper = cartridge->mapper();

    WriteRegister(mapper, submapper, 0x8000, 3);
    WriteRegister(mapper, submapper, 0x8010, 4);
    WriteRegister(mapper, submapper, 0x9000, 5);
    EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(5 * k8K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xe123), TestPRGByte(63 * k8K + 0x0123));

    for (Byte window = 0; window < 8; ++window) {
      const Address address = static_cast<Address>(
          0xa000 + (window / 2) * 0x1000 + (window & 1) * 0x0010);
      WriteRegister(mapper, submapper, address,
                    static_cast<Byte>(0x10 + window));
    }
    for (size_t window = 0; window < 8; ++window) {
      const Address address = static_cast<Address>(window * k1K + 0x0123);
      const size_t expected = (0x10 + window) * k1K + 0x0123;
      EXPECT_EQ(mapper->ReadCHR(address), TestCHRByte(expected));
      EXPECT_EQ(mapper->GetAbsoluteCHRAddress(address), expected);
    }
  }
}

TEST_F(Mapper085Test, HonorsA3AndA4SubmapperRegisterDecoding) {
  auto vrc7b_cartridge = LoadMapper(85, 32, 32, 0, 1);
  ASSERT_TRUE(vrc7b_cartridge);
  Mapper* vrc7b = vrc7b_cartridge->mapper();
  vrc7b->WritePRG(0x8010, 2);
  vrc7b->WritePRG(0x8008, 3);
  EXPECT_EQ(vrc7b->ReadPRG(0x8123), TestPRGByte(2 * k8K + 0x0123));
  EXPECT_EQ(vrc7b->ReadPRG(0xa123), TestPRGByte(3 * k8K + 0x0123));

  auto vrc7a_cartridge = LoadMapper(85, 32, 32, 0, 2);
  ASSERT_TRUE(vrc7a_cartridge);
  Mapper* vrc7a = vrc7a_cartridge->mapper();
  vrc7a->WritePRG(0x8008, 4);
  vrc7a->WritePRG(0x8010, 5);
  EXPECT_EQ(vrc7a->ReadPRG(0x8123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(vrc7a->ReadPRG(0xa123), TestPRGByte(5 * k8K + 0x0123));
}

TEST_F(Mapper085Test, UsesBothRegisterLinesForLegacyINESImages) {
  auto cartridge = LoadMapper(85, 32, 32);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8008, 3);
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(3 * k8K + 0x0123));
  mapper->WritePRG(0x8010, 4);
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
}

TEST_F(Mapper085Test, ControlsMirroringAndProgramRAMAccess) {
  auto cartridge = LoadMapper(85, 32, 32, 0, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr std::array<NametableMirroring, 4> kExpected = {
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
  };
  for (Byte mode = 0; mode < kExpected.size(); ++mode) {
    mapper->WritePRG(0xe000, static_cast<Byte>(0x80 | mode));
    EXPECT_EQ(mapper->GetNametableMirroring(), kExpected[mode]);
  }
  EXPECT_EQ(mirroring_changes_, 3);

  mapper->WriteExtendedRAM(0x6123, 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  mapper->WritePRG(0xe000, 0x00);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
  mapper->WritePRG(0xe000, 0x80);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
}

TEST_F(Mapper085Test, BanksCharacterRAM) {
  auto cartridge = LoadMapper(85, 8, 0, 0, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0xa000, 5);
  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WritePRG(0xa000, 1);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0);
  mapper->WriteCHR(0x0123, 0xa5);
  mapper->WritePRG(0xa000, 5);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 5 * k1K + 0x0123);
}

TEST_F(Mapper085Test, CountsCycleAndScanlineIRQsForBothWirings) {
  for (Byte submapper : {1, 2}) {
    auto cartridge = LoadMapper(85, 32, 32, 0, submapper);
    ASSERT_TRUE(cartridge);
    Mapper* mapper = cartridge->mapper();
    ASSERT_TRUE(mapper->NeedsM2CycleIRQ());
    const int initial_irq_count = irq_count_;

    WriteRegister(mapper, submapper, 0xe010, 0xfe);
    mapper->WritePRG(0xf000, 0x07);
    AdvanceCycles(mapper, 2);
    EXPECT_EQ(irq_count_, initial_irq_count + 1);
    WriteRegister(mapper, submapper, 0xf010, 0);
    AdvanceCycles(mapper, 2);
    EXPECT_EQ(irq_count_, initial_irq_count + 2);

    WriteRegister(mapper, submapper, 0xe010, 0xff);
    mapper->WritePRG(0xf000, 0x02);
    AdvanceCycles(mapper, 113);
    EXPECT_EQ(irq_count_, initial_irq_count + 2);
    mapper->M2CycleIRQ();
    EXPECT_EQ(irq_count_, initial_irq_count + 3);
  }
}

TEST_F(Mapper085Test, ProducesVRC7aAudioAndSilencesItWhileReset) {
  auto cartridge = LoadMapper(85, 32, 32, 0, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  Blip_Buffer buffer;
  ASSERT_EQ(buffer.sample_rate(44100, 100), nullptr);
  buffer.clock_rate(1789773);
  mapper->SetExpansionAudioOutput(&buffer);
  mapper->SetExpansionAudioVolume(1.f);

  mapper->WritePRG(0xe000, 0x40);
  WriteAudioRegister(mapper, 0x30, 0x10);
  WriteAudioRegister(mapper, 0x10, 0x80);
  WriteAudioRegister(mapper, 0x20, 0x15);
  mapper->EndExpansionAudioFrame(30000);
  buffer.end_frame(30000);
  EXPECT_FALSE(HasNonzeroSamples(&buffer));

  mapper->WritePRG(0xe000, 0x00);
  WriteAudioRegister(mapper, 0x30, 0x10);
  WriteAudioRegister(mapper, 0x10, 0x80);
  WriteAudioRegister(mapper, 0x20, 0x15);
  mapper->EndExpansionAudioFrame(30000);
  buffer.end_frame(30000);
  EXPECT_TRUE(HasNonzeroSamples(&buffer));
}

TEST_F(Mapper085Test, SilencesActiveVRC7AudioAtResetCycle) {
  auto cartridge = LoadMapper(85, 32, 32, 0, 2);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  Blip_Buffer buffer;
  ASSERT_EQ(buffer.sample_rate(44100, 100), nullptr);
  buffer.clock_rate(1789773);
  buffer.bass_freq(0);
  mapper->SetExpansionAudioOutput(&buffer);
  mapper->SetExpansionAudioVolume(1.f);

  WriteAudioRegister(mapper, 0x30, 0x10);
  WriteAudioRegister(mapper, 0x10, 0x80);
  WriteAudioRegister(mapper, 0x20, 0x15);
  mapper->EndExpansionAudioFrame(30000);
  buffer.end_frame(30000);
  ASSERT_TRUE(HasNonzeroSamples(&buffer));

  constexpr int kResetCycle = 12000;
  AdvanceCycles(mapper, kResetCycle);
  const size_t reset_sample = buffer.count_samples(kResetCycle);
  mapper->WritePRG(0xe000, 0x40);
  AdvanceCycles(mapper, 30000 - kResetCycle);
  mapper->EndExpansionAudioFrame(30000);
  buffer.end_frame(30000);

  std::vector<blip_sample_t> samples(buffer.samples_avail());
  ASSERT_GT(samples.size(),
            reset_sample + static_cast<size_t>(buffer.output_latency()) + 32);
  buffer.read_samples(samples.data(), samples.size());
  EXPECT_TRUE(std::any_of(samples.begin(), samples.begin() + reset_sample,
                          [](blip_sample_t sample) { return sample != 0; }));
  const auto silence_begin =
      samples.begin() + reset_sample + buffer.output_latency() + 32;
  EXPECT_TRUE(std::all_of(silence_begin, samples.end(),
                          [](blip_sample_t sample) { return sample == 0; }));
}

TEST_F(Mapper085Test, DoesNotOutputExpansionAudioForVRC7b) {
  auto cartridge = LoadMapper(85, 32, 32, 0, 1);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  Blip_Buffer buffer;
  ASSERT_EQ(buffer.sample_rate(44100, 100), nullptr);
  buffer.clock_rate(1789773);
  mapper->SetExpansionAudioOutput(&buffer);
  mapper->SetExpansionAudioVolume(1.f);
  WriteAudioRegister(mapper, 0x30, 0x10);
  WriteAudioRegister(mapper, 0x10, 0x80);
  WriteAudioRegister(mapper, 0x20, 0x15);
  mapper->EndExpansionAudioFrame(30000);
  buffer.end_frame(30000);

  EXPECT_FALSE(HasNonzeroSamples(&buffer));
}

TEST_F(Mapper085Test, RestoresBanksIRQProgramRAMAndCharacterRAM) {
  auto cartridge = LoadMapper(85, 32, 0, 0x02, 2, 0, 0x70);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WritePRG(0x8000, 3);
  mapper->WritePRG(0x8010, 4);
  mapper->WritePRG(0xa000, 5);
  mapper->WritePRG(0xe000, 0x82);
  mapper->WriteExtendedRAM(0x6123, 0x5a);
  mapper->WriteCHR(0x0123, 0x45);
  mapper->WritePRG(0xe010, 0xff);
  mapper->WritePRG(0xf000, 0x06);
  AdvanceCycles(mapper, 1);
  const Bytes state = SerializeMapper(mapper);

  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0x8010, 2);
  mapper->WritePRG(0xa000, 1);
  mapper->WritePRG(0xe000, 0x80);
  mapper->WriteExtendedRAM(0x6123, 0xa5);
  mapper->WriteCHR(0x0123, 0x67);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(3 * k8K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xa123), TestPRGByte(4 * k8K + 0x0123));
  EXPECT_EQ(mapper->GetAbsoluteCHRAddress(0x0123), 5 * k1K + 0x0123);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x45);
  EXPECT_EQ(mapper->GetNametableMirroring(),
            NametableMirroring::kOneScreenLower);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x5a);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 2);
}

TEST_F(Mapper085Test, RendersBothVRC7ROMsFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  const std::map<std::string, std::pair<uint32_t, Byte>> expected_roms{
      {"Lagrange Point (Japan).nes", {0x33ce3ff0u, 2}},
      {"Tiny Toon Adventures 2 - Montana Land he Youkoso (Japan).nes",
       {0xe4362167u, 1}},
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
    ASSERT_EQ(emulator_->GetRomData()->crc, expected->second.first)
        << entry.path();
    ASSERT_EQ(emulator_->GetRomData()->mapper, 85) << entry.path();
    ASSERT_EQ(emulator_->GetRomData()->submapper, expected->second.second)
        << entry.path();

    emulator_->Run();
    Colors previous_pixels;
    bool rendered = false;
    bool frame_changed = false;
    for (int frame = 0; frame < 600; ++frame) {
      emulator_->RunOneFrame();
      const Colors& pixels = emulator_->GetLastFrame();
      ASSERT_EQ(pixels.size(), 256u * 240u) << entry.path();
      const std::set<Color> unique_colors(pixels.begin(), pixels.end());
      rendered = rendered || unique_colors.size() > 1;
      if (!previous_pixels.empty() && pixels != previous_pixels)
        frame_changed = true;
      previous_pixels = pixels;
    }
    EXPECT_TRUE(rendered) << entry.path();
    EXPECT_TRUE(frame_changed) << entry.path();
    ++verified_roms;
  }

  EXPECT_EQ(verified_roms, expected_roms.size());
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
