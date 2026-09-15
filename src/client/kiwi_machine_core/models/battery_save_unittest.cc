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

#include "models/battery_save.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_executor.h"
#include "nes/emulator.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace {

constexpr size_t kINESHeaderSize = 16;
constexpr size_t kPRGROMSize = 32 * 1024;
constexpr size_t kCHRROMSize = 8 * 1024;
constexpr size_t kPRGNVRAMSize = 8 * 1024;

kiwi::nes::Bytes MakeBatteryROM() {
  kiwi::nes::Bytes rom(kINESHeaderSize + kPRGROMSize + kCHRROMSize);
  rom[0] = 'N';
  rom[1] = 'E';
  rom[2] = 'S';
  rom[3] = 0x1a;
  rom[4] = 2;
  rom[5] = 1;
  rom[6] = 0x02;
  return rom;
}

class BatterySaveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::string directory_name =
        "kiwi_battery_save_" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    profile_path_ = kiwi::base::FilePath::FromUTF8Unsafe(
        (std::filesystem::temp_directory_path() / directory_name).string());
    ASSERT_TRUE(kiwi::base::CreateDirectory(profile_path_));
  }

  void TearDown() override {
    EXPECT_TRUE(kiwi::base::DeletePathRecursively(profile_path_));
  }

  kiwi::base::FilePath profile_path_;
};

class BatterySaveEmulatorIntegrationTest : public BatterySaveTest {
 protected:
  void SetUp() override {
    BatterySaveTest::SetUp();
    task_executor_ = std::make_unique<kiwi::base::SingleThreadTaskExecutor>();
    emulator_ = kiwi::nes::CreateEmulatorForTesting();
    emulator_->PowerOn();
  }

  void TearDown() override {
    emulator_->PowerOff();
    emulator_.reset();
    task_executor_.reset();
    BatterySaveTest::TearDown();
  }

  std::unique_ptr<kiwi::base::SingleThreadTaskExecutor> task_executor_;
  scoped_refptr<kiwi::nes::Emulator> emulator_;
};

TEST_F(BatterySaveTest, ReadsWritesAndAtomicallyReplacesRawSave) {
  const std::string lowercase_sha1(40, 'a');
  const std::string uppercase_sha1(40, 'A');
  std::optional<kiwi::base::FilePath> resolved_path =
      battery_save::GetPath(profile_path_, lowercase_sha1);
  ASSERT_TRUE(resolved_path);
  const kiwi::base::FilePath save_path = *resolved_path;
  const kiwi::base::FilePath temporary_path =
      kiwi::base::FilePath::FromUTF8Unsafe(save_path.AsUTF8Unsafe() + ".tmp");

  battery_save::ReadResult missing =
      battery_save::Read(profile_path_, uppercase_sha1);
  EXPECT_TRUE(missing.success);
  EXPECT_FALSE(missing.exists);

  const kiwi::nes::Bytes first{1, 2, 3, 4};
  ASSERT_TRUE(battery_save::Write(profile_path_, lowercase_sha1, first));
  EXPECT_TRUE(kiwi::base::PathExists(save_path));
  EXPECT_FALSE(kiwi::base::PathExists(temporary_path));

  battery_save::ReadResult loaded = battery_save::ReadFromPath(save_path);
  ASSERT_TRUE(loaded.success);
  ASSERT_TRUE(loaded.exists);
  EXPECT_EQ(loaded.data, first);

  const kiwi::nes::Bytes second{5, 6, 7, 8};
  ASSERT_TRUE(battery_save::Write(profile_path_, uppercase_sha1, second));
  loaded = battery_save::Read(profile_path_, lowercase_sha1);
  ASSERT_TRUE(loaded.success);
  EXPECT_EQ(loaded.data, second);
}

TEST_F(BatterySaveTest, WritesExplicitPathAndBacksUpInvalidSave) {
  const kiwi::base::FilePath save_path =
      profile_path_.Append(FILE_PATH_LITERAL("External"))
          .Append(FILE_PATH_LITERAL("game.sav"));
  const kiwi::nes::Bytes data{1, 2, 3, 4};
  ASSERT_TRUE(battery_save::WriteToPath(save_path, data));

  std::optional<kiwi::base::FilePath> first_backup =
      battery_save::BackupInvalid(save_path);
  ASSERT_TRUE(first_backup);
  EXPECT_TRUE(kiwi::base::PathExists(save_path));
  battery_save::ReadResult loaded = battery_save::ReadFromPath(*first_backup);
  ASSERT_TRUE(loaded.success);
  EXPECT_EQ(loaded.data, data);

  std::optional<kiwi::base::FilePath> second_backup =
      battery_save::BackupInvalid(save_path);
  ASSERT_TRUE(second_backup);
  EXPECT_NE(*first_backup, *second_backup);
  EXPECT_TRUE(kiwi::base::PathExists(*first_backup));
  EXPECT_TRUE(kiwi::base::PathExists(*second_backup));
}

TEST_F(BatterySaveTest, RejectsInvalidSHA1AndEmptyData) {
  EXPECT_FALSE(
      battery_save::Write(profile_path_, "../invalid", kiwi::nes::Bytes{1}));
  EXPECT_FALSE(battery_save::Write(profile_path_, std::string(40, '0'), {}));

  battery_save::ReadResult invalid =
      battery_save::Read(profile_path_, std::string(40, 'z'));
  EXPECT_FALSE(invalid.success);
  EXPECT_FALSE(invalid.exists);
}

TEST_F(BatterySaveEmulatorIntegrationTest,
       TransfersExplicitSaveBetweenFileAndEmulator) {
  const kiwi::base::FilePath save_path =
      profile_path_.Append(FILE_PATH_LITERAL("External"))
          .Append(FILE_PATH_LITERAL("game.sav"));
  kiwi::nes::Bytes expected(kPRGNVRAMSize);
  expected[0x123] = 0x5a;
  ASSERT_TRUE(battery_save::WriteToPath(save_path, expected));

  battery_save::ReadResult loaded_save = battery_save::ReadFromPath(save_path);
  ASSERT_TRUE(loaded_save.success);
  ASSERT_TRUE(loaded_save.exists);

  bool loaded = false;
  emulator_->LoadAndRunWithPRGNVRAM(
      MakeBatteryROM(), loaded_save.data,
      kiwi::base::BindOnce(
          [](bool* loaded, bool success) { *loaded = success; }, &loaded));
  ASSERT_TRUE(loaded);

  std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot;
  emulator_->ExportPRGNVRAM(
      false,
      kiwi::base::BindOnce(
          [](std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot>* output,
             std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> value) {
            *output = std::move(value);
          },
          &snapshot));
  ASSERT_TRUE(snapshot);
  EXPECT_EQ(snapshot->data, expected);
  ASSERT_TRUE(battery_save::WriteToPath(save_path, snapshot->data));

  battery_save::ReadResult persisted = battery_save::ReadFromPath(save_path);
  ASSERT_TRUE(persisted.success);
  EXPECT_EQ(persisted.data, expected);
}

}  // namespace
