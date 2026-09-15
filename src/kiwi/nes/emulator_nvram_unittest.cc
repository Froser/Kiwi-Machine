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

#include "nes/emulator.h"

#include <optional>
#include <span>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "nes/mappers/mapper_test_support.h"
#include "nes/rom_data.h"
#include "nes/rom_hash.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace kiwi {
namespace nes {
namespace testing {
namespace {

constexpr size_t k8K = 0x2000;

}  // namespace

class EmulatorNVRAMTest : public ::testing::Test {
 protected:
  void SetUp() override {
    task_executor_ = std::make_unique<base::SingleThreadTaskExecutor>();
    emulator_ = CreateEmulatorForTesting();
    emulator_->PowerOn();
  }

  void TearDown() override { emulator_->PowerOff(); }

  std::unique_ptr<base::SingleThreadTaskExecutor> task_executor_;
  scoped_refptr<Emulator> emulator_;
};

TEST_F(EmulatorNVRAMTest, ComputesROMHashAndTransfersPRGNVRAM) {
  Bytes rom = MakeTestROM(0, 2, 1, 0x02);
  Bytes initial_nvram(k8K);
  initial_nvram[0x123] = 0x3c;

  bool loaded = false;
  emulator_->LoadAndRunWithPRGNVRAM(
      rom, initial_nvram,
      base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                     &loaded));
  ASSERT_TRUE(loaded);

  const RomData* rom_data = emulator_->GetRomData();
  ASSERT_NE(rom_data, nullptr);
  const std::string expected_sha1 =
      CalculateSha1Hex(std::span<const Byte>(rom).subspan(16));
  EXPECT_EQ(rom_data->sha1, expected_sha1);

  std::optional<Emulator::PRGNVRAMSnapshot> initial_snapshot;
  emulator_->ExportPRGNVRAM(
      false, base::BindOnce(
                 [](std::optional<Emulator::PRGNVRAMSnapshot>* output,
                    std::optional<Emulator::PRGNVRAMSnapshot> value) {
                   *output = std::move(value);
                 },
                 &initial_snapshot));
  ASSERT_TRUE(initial_snapshot);
  EXPECT_EQ(initial_snapshot->rom_sha1, expected_sha1);
  EXPECT_EQ(initial_snapshot->data, initial_nvram);

  std::optional<Emulator::PRGNVRAMSnapshot> dirty_snapshot;
  emulator_->ExportPRGNVRAM(
      true, base::BindOnce(
                [](std::optional<Emulator::PRGNVRAMSnapshot>* output,
                   std::optional<Emulator::PRGNVRAMSnapshot> value) {
                  *output = std::move(value);
                },
                &dirty_snapshot));
  EXPECT_FALSE(dirty_snapshot);
}

TEST_F(EmulatorNVRAMTest, RejectsInvalidInitialPRGNVRAMBeforeReplacingROM) {
  Bytes first_rom = MakeTestROM(0, 2, 1, 0x02);
  bool first_loaded = false;
  emulator_->LoadFromBinary(
      first_rom,
      base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                     &first_loaded));
  ASSERT_TRUE(first_loaded);
  const RomData* previous_rom = emulator_->GetRomData();

  Bytes second_rom = MakeTestROM(0, 1, 1, 0x02);
  Bytes invalid_nvram(1);
  bool second_loaded = true;
  emulator_->LoadAndRunWithPRGNVRAM(
      second_rom, invalid_nvram,
      base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                     &second_loaded));

  EXPECT_FALSE(second_loaded);
  EXPECT_EQ(emulator_->GetRomData(), previous_rom);
}

TEST_F(EmulatorNVRAMTest, RejectsTruncatedBinaryROM) {
  bool loaded = true;
  emulator_->LoadFromBinary(
      Bytes{'N', 'E', 'S', 0x1a},
      base::BindOnce([](bool* loaded, bool success) { *loaded = success; },
                     &loaded));
  EXPECT_FALSE(loaded);
  EXPECT_EQ(emulator_->GetRomData(), nullptr);
}

}  // namespace testing
}  // namespace nes
}  // namespace kiwi
