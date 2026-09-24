// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper_test_support.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <set>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k16K = 0x4000;
constexpr uint32_t kIRQThreshold = 0x20000000;

void UnlockMapper(Mapper* mapper, Byte event_register) {
  WriteMMC1Register(mapper, 0xa000, event_register & 0x0f);
  WriteMMC1Register(mapper, 0xa000, event_register | 0x10);
}

void WriteMMC1RegisterWithDuplicates(Mapper* mapper,
                                     Address address,
                                     Byte value) {
  for (int bit = 0; bit < 5; ++bit) {
    const Byte bit_value = (value >> bit) & 1;
    mapper->WritePRG(address, bit_value);
    mapper->WritePRG(address, bit_value ^ 1);
    mapper->M2CycleIRQ();
  }
}

uint32_t ReadIRQCounter(const Bytes& state) {
  EXPECT_GE(state.size(), sizeof(uint32_t));
  uint32_t counter = 0;
  if (state.size() >= sizeof(counter))
    std::memcpy(&counter, state.data(), sizeof(counter));
  return counter;
}

void SetIRQCounter(Bytes* state, uint32_t counter) {
  ASSERT_TRUE(state);
  ASSERT_GE(state->size(), sizeof(counter));
  std::memcpy(state->data(), &counter, sizeof(counter));
}

}  // namespace

class Mapper105Test : public MapperTest {};

TEST_F(Mapper105Test, CreatesNESEventWithFixedInitialBanksAndWorkRAM) {
  EXPECT_TRUE(Mapper::IsMapperSupported(105));

  auto cartridge = LoadMapper(105, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  EXPECT_TRUE(mapper->NeedsM2CycleIRQ());
  EXPECT_TRUE(mapper->HasPRGRAM());
  EXPECT_FALSE(mapper->HasBatteryBackedRAM());
  EXPECT_EQ(cartridge->GetRomData()->prg_ram_size, 0x2000u);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(k16K + 0x0123));

  WriteMMC1Register(mapper, 0x8000, 0x00);
  WriteMMC1Register(mapper, 0xa000, 0x16);
  WriteMMC1Register(mapper, 0xe000, 7);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(k16K + 0x0123));
}

TEST_F(Mapper105Test, UnlocksOnlyAfterLowThenHighITransition) {
  auto cartridge = LoadMapper(105, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC1Register(mapper, 0xa000, 0x16);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));

  WriteMMC1Register(mapper, 0xa000, 0x06);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(0));

  WriteMMC1Register(mapper, 0xa000, 0x16);
  EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(6 * k16K + 0x0123));
  EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte(7 * k16K + 0x0123));

  constexpr std::array<Byte, 4> kLowerBanks = {0, 2, 4, 6};
  for (Byte bank : kLowerBanks) {
    WriteMMC1Register(mapper, 0xa000, static_cast<Byte>(0x10 | bank));
    EXPECT_EQ(mapper->ReadPRG(0x8123), TestPRGByte(bank * k16K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xc123), TestPRGByte((bank + 1) * k16K + 0x0123));
  }
}

TEST_F(Mapper105Test, MapsUpperChipInAllMMC1ProgramModes) {
  struct TestCase {
    Byte control;
    Byte prg;
    size_t lower_bank;
    size_t upper_bank;
  };
  constexpr std::array<TestCase, 4> kCases{{
      {0x00, 5, 12, 13},
      {0x04, 5, 12, 13},
      {0x08, 5, 8, 13},
      {0x0c, 5, 13, 15},
  }};

  for (const auto& test : kCases) {
    auto cartridge = LoadMapper(105, 16, 0);
    ASSERT_TRUE(cartridge);
    Mapper* mapper = cartridge->mapper();
    UnlockMapper(mapper, 0x08);
    WriteMMC1Register(mapper, 0x8000, test.control);
    WriteMMC1Register(mapper, 0xe000, test.prg);

    EXPECT_EQ(mapper->ReadPRG(0x8123),
              TestPRGByte(test.lower_bank * k16K + 0x0123));
    EXPECT_EQ(mapper->ReadPRG(0xc123),
              TestPRGByte(test.upper_bank * k16K + 0x0123));
  }
}

TEST_F(Mapper105Test, ControlsMirroringWithoutChangingItOnResetWrite) {
  auto cartridge = LoadMapper(105, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  constexpr std::array<NametableMirroring, 4> kExpected = {
      NametableMirroring::kOneScreenLower,
      NametableMirroring::kOneScreenHigher,
      NametableMirroring::kVertical,
      NametableMirroring::kHorizontal,
  };
  for (Byte mode = 0; mode < kExpected.size(); ++mode) {
    WriteMMC1Register(mapper, 0x8000, static_cast<Byte>(0x0c | mode));
    EXPECT_EQ(mapper->GetNametableMirroring(), kExpected[mode]);
  }
  EXPECT_EQ(mirroring_changes_, 3);

  mapper->WritePRG(0x8000, 0x80);
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
}

TEST_F(Mapper105Test, UsesUnbankedCharacterRAMAndGatesWorkRAM) {
  auto cartridge = LoadMapper(105, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  mapper->WriteCHR(0x0123, 0x5a);
  WriteMMC1Register(mapper, 0xa000, 0x07);
  WriteMMC1Register(mapper, 0xc000, 0x1f);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  mapper->WriteCHR(0x2123, 0xa5);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0xa5);

  mapper->WriteExtendedRAM(0x6123, 0x45);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x45);
  WriteMMC1Register(mapper, 0xe000, 0x10);
  mapper->WriteExtendedRAM(0x6123, 0x67);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x61);
  WriteMMC1Register(mapper, 0xe000, 0x00);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x45);
}

TEST_F(Mapper105Test, SuppressesConsecutiveWritesAndHonorsResetBypass) {
  auto cartridge = LoadMapper(105, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  WriteMMC1Register(mapper, 0xa000, 0x06);
  WriteMMC1RegisterWithDuplicates(mapper, 0xa000, 0x16);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(6 * k16K));

  WriteMMC1Register(mapper, 0xa000, 0x18);
  WriteMMC1Register(mapper, 0x8000, 0x08);
  WriteMMC1Register(mapper, 0xe000, 3);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(8 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(11 * k16K));

  mapper->WritePRG(0x8000, 1);
  mapper->WritePRG(0x8000, 0x80);
  mapper->M2CycleIRQ();
  WriteMMC1Register(mapper, 0xe000, 2);
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(10 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(15 * k16K));
}

TEST_F(Mapper105Test, ClocksResetsAndStopsEventTimer) {
  auto cartridge = LoadMapper(105, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  const Bytes disabled_state = SerializeMapper(mapper);
  mapper->M2CycleIRQ();
  mapper->M2CycleIRQ();
  EXPECT_EQ(ReadIRQCounter(SerializeMapper(mapper)),
            ReadIRQCounter(disabled_state));

  WriteMMC1Register(mapper, 0xa000, 0x00);
  const uint32_t enabled_counter = ReadIRQCounter(SerializeMapper(mapper));
  mapper->M2CycleIRQ();
  mapper->M2CycleIRQ();
  EXPECT_EQ(ReadIRQCounter(SerializeMapper(mapper)), enabled_counter + 2);

  Bytes near_threshold = SerializeMapper(mapper);
  SetIRQCounter(&near_threshold, kIRQThreshold - 1);
  ASSERT_TRUE(DeserializeMapper(mapper, near_threshold));
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);

  WriteMMC1Register(mapper, 0xa000, 0x10);
  EXPECT_EQ(ReadIRQCounter(SerializeMapper(mapper)), 0u);
  mapper->M2CycleIRQ();
  EXPECT_EQ(irq_count_, 1);
}

TEST_F(Mapper105Test, PersistsBanksTimerMirroringAndRAM) {
  auto cartridge = LoadMapper(105, 16, 0);
  ASSERT_TRUE(cartridge);
  Mapper* mapper = cartridge->mapper();

  UnlockMapper(mapper, 0x08);
  WriteMMC1Register(mapper, 0x8000, 0x0f);
  WriteMMC1Register(mapper, 0xe000, 4);
  mapper->WriteCHR(0x0123, 0x5a);
  mapper->WriteExtendedRAM(0x6123, 0x45);
  WriteMMC1Register(mapper, 0xa000, 0x08);
  for (int cycle = 0; cycle < 7; ++cycle)
    mapper->M2CycleIRQ();
  const Bytes state = SerializeMapper(mapper);
  const uint32_t saved_counter = ReadIRQCounter(state);

  WriteMMC1Register(mapper, 0xa000, 0x12);
  WriteMMC1Register(mapper, 0x8000, 0x00);
  WriteMMC1Register(mapper, 0xe000, 0x10);
  mapper->WriteCHR(0x0123, 0xa5);

  ASSERT_TRUE(DeserializeMapper(mapper, state));
  EXPECT_EQ(mapper->ReadPRG(0x8000), TestPRGByte(12 * k16K));
  EXPECT_EQ(mapper->ReadPRG(0xc000), TestPRGByte(15 * k16K));
  EXPECT_EQ(mapper->GetNametableMirroring(), NametableMirroring::kHorizontal);
  EXPECT_EQ(mapper->ReadCHR(0x0123), 0x5a);
  EXPECT_EQ(mapper->ReadExtendedRAM(0x6123), 0x45);

  mapper->M2CycleIRQ();
  EXPECT_EQ(ReadIRQCounter(SerializeMapper(mapper)), saved_counter + 1);
}

TEST_F(Mapper105Test, RendersNintendoWorldChampionshipsFor600Frames) {
  const char* rom_directory = std::getenv("KIWI_NES_ACCEPTANCE_ROM_DIR");
  if (!rom_directory || *rom_directory == '\0') {
    GTEST_SKIP() << "Set KIWI_NES_ACCEPTANCE_ROM_DIR to run real-ROM checks.";
  }

  size_t verified_roms = 0;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(rom_directory)) {
    if (!entry.is_regular_file() ||
        entry.path().filename() !=
            "Nintendo World Championships 1990 (USA).nes") {
      continue;
    }

    bool loaded = false;
    emulator_->LoadFromFile(
        base::FilePath::FromUTF8Unsafe(entry.path().string()),
        base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                       base::Unretained(&loaded)));
    ASSERT_TRUE(loaded) << entry.path();
    ASSERT_TRUE(emulator_->GetRomData());
    EXPECT_EQ(emulator_->GetRomData()->crc, 0x0b0e128fu);
    EXPECT_EQ(emulator_->GetRomData()->mapper, 105);

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
