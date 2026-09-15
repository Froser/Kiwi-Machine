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

#include "models/nes_runtime.h"

#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/runloop.h"
#include "base/task/single_thread_task_executor.h"
#include "models/battery_save.h"
#include "nes/rom_hash.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace {

constexpr size_t kINESHeaderSize = 16;
constexpr size_t kPRGROMSize = 32 * 1024;
constexpr size_t kCHRROMSize = 8 * 1024;
constexpr size_t kPRGNVRAMSize = 8 * 1024;
constexpr kiwi::nes::Address kTestAddress = 0x6123;
constexpr size_t kTestOffset = kTestAddress - 0x6000;

kiwi::nes::Bytes MakeWritingBatteryROM(kiwi::nes::Byte initial_value = 0) {
  kiwi::nes::Bytes rom(kINESHeaderSize + kPRGROMSize + kCHRROMSize);
  rom[0] = 'N';
  rom[1] = 'E';
  rom[2] = 'S';
  rom[3] = 0x1a;
  rom[4] = 2;
  rom[5] = 1;
  rom[6] = 0x02;

  size_t program = kINESHeaderSize;
  rom[program++] = 0xa9;  // LDA #initial_value
  rom[program++] = initial_value;
  rom[program++] = 0x8d;  // STA $6123
  rom[program++] = 0x23;
  rom[program++] = 0x61;
  rom[program++] = 0xee;  // INC $6123
  rom[program++] = 0x23;
  rom[program++] = 0x61;
  rom[program++] = 0x4c;  // JMP $8005
  rom[program++] = 0x05;
  rom[program++] = 0x80;

  const size_t vectors = kINESHeaderSize + kPRGROMSize - 6;
  for (size_t i = 0; i < 3; ++i) {
    rom[vectors + i * 2] = 0x00;
    rom[vectors + i * 2 + 1] = 0x80;
  }
  return rom;
}

class DeferredTaskRunner final : public kiwi::base::SequencedTaskRunner {
 public:
  bool PostDelayedTask(const kiwi::base::Location&,
                       kiwi::base::OnceClosure task,
                       kiwi::base::TimeDelta) override {
    tasks_.push_back(std::move(task));
    return true;
  }

  bool PostTaskAndReply(const kiwi::base::Location&,
                        kiwi::base::OnceClosure task,
                        kiwi::base::OnceClosure reply) override {
    tasks_.push_back(std::move(task).Then(std::move(reply)));
    return true;
  }

  size_t pending_task_count() const { return tasks_.size(); }

  void RunNextTask() {
    ASSERT_FALSE(tasks_.empty());
    kiwi::base::OnceClosure task = std::move(tasks_.front());
    tasks_.pop_front();
    std::move(task).Run();
  }

  void RunUntilIdle() {
    while (!tasks_.empty()) {
      RunNextTask();
    }
  }

 private:
  friend class kiwi::base::RefCountedThreadSafe<DeferredTaskRunner>;
  ~DeferredTaskRunner() override = default;

  std::deque<kiwi::base::OnceClosure> tasks_;
};

class NESRuntimeBatterySaveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    task_executor_ = std::make_unique<kiwi::base::SingleThreadTaskExecutor>();
    io_task_runner_ = kiwi::base::MakeRefCounted<DeferredTaskRunner>();
    timer_task_runner_ = kiwi::base::MakeRefCounted<DeferredTaskRunner>();
    runtime_ =
        std::make_unique<NESRuntime::Data>(io_task_runner_, timer_task_runner_);
    runtime_->emulator = kiwi::nes::CreateEmulatorForTesting();
    runtime_->emulator->PowerOn();

    const std::string directory_name =
        "kiwi_runtime_battery_save_" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    runtime_->profile_path = kiwi::base::FilePath::FromUTF8Unsafe(
        (std::filesystem::temp_directory_path() / directory_name).string());
    ASSERT_TRUE(kiwi::base::CreateDirectory(runtime_->profile_path));
  }

  void TearDown() override {
    EXPECT_EQ(io_task_runner_->pending_task_count(), 0u);
    EXPECT_EQ(timer_task_runner_->pending_task_count(), 0u);
    runtime_->emulator->PowerOff();
    EXPECT_TRUE(kiwi::base::DeletePathRecursively(runtime_->profile_path));
    runtime_.reset();
    io_task_runner_.reset();
    timer_task_runner_.reset();
    task_executor_.reset();
  }

  kiwi::base::FilePath WriteROM(const kiwi::nes::Bytes& rom, const char* name) {
    kiwi::base::FilePath path = runtime_->profile_path.Append(
        kiwi::base::FilePath::FromUTF8Unsafe(name));
    EXPECT_TRUE(kiwi::base::WriteFile(path, rom));
    return path;
  }

  bool LoadROM(
      const kiwi::base::FilePath& rom_path,
      const std::optional<kiwi::base::FilePath>& save_path = std::nullopt) {
    bool callback_called = false;
    bool success = false;
    runtime_->LoadROM(
        rom_path,
        kiwi::base::BindOnce(
            [](bool* callback_called, bool* success, bool result) {
              *callback_called = true;
              *success = result;
            },
            &callback_called, &success),
        save_path);
    io_task_runner_->RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return success;
  }

  bool FlushBatterySave() {
    bool callback_called = false;
    bool success = false;
    runtime_->FlushBatterySave(kiwi::base::BindOnce(
        [](bool* callback_called, bool* success, bool result) {
          *callback_called = true;
          *success = result;
        },
        &callback_called, &success));
    EXPECT_FALSE(callback_called);
    io_task_runner_->RunNextTask();
    EXPECT_TRUE(callback_called);
    return success;
  }

  std::unique_ptr<kiwi::base::SingleThreadTaskExecutor> task_executor_;
  scoped_refptr<DeferredTaskRunner> io_task_runner_;
  scoped_refptr<DeferredTaskRunner> timer_task_runner_;
  std::unique_ptr<NESRuntime::Data> runtime_;
};

TEST_F(NESRuntimeBatterySaveTest,
       PreservesDirtyGenerationAcrossAsynchronousExplicitPathWrite) {
  kiwi::nes::Bytes rom = MakeWritingBatteryROM(0x10);
  const kiwi::base::FilePath rom_path = WriteROM(rom, "first.nes");
  const kiwi::base::FilePath explicit_save_path =
      runtime_->profile_path.Append(FILE_PATH_LITERAL("External"))
          .Append(FILE_PATH_LITERAL("first.sav"));
  ASSERT_TRUE(battery_save::WriteToPath(explicit_save_path,
                                        kiwi::nes::Bytes(kPRGNVRAMSize)));
  ASSERT_TRUE(LoadROM(rom_path, explicit_save_path));

  runtime_->emulator->RunOneFrame();
  bool first_flush_called = false;
  runtime_->FlushBatterySave(kiwi::base::BindOnce(
      [](bool* called, bool success) {
        EXPECT_TRUE(success);
        *called = true;
      },
      &first_flush_called));
  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);

  runtime_->emulator->RunOneFrame();
  io_task_runner_->RunNextTask();
  EXPECT_TRUE(first_flush_called);

  std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> latest_snapshot;
  runtime_->emulator->ExportPRGNVRAM(
      true,
      kiwi::base::BindOnce(
          [](std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot>* output,
             std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot) {
            *output = std::move(snapshot);
          },
          &latest_snapshot));
  ASSERT_TRUE(latest_snapshot);

  battery_save::ReadResult first_persisted =
      battery_save::ReadFromPath(explicit_save_path);
  ASSERT_TRUE(first_persisted.success);
  EXPECT_NE(first_persisted.data[kTestOffset],
            latest_snapshot->data[kTestOffset]);
  EXPECT_TRUE(FlushBatterySave());

  battery_save::ReadResult latest_persisted =
      battery_save::ReadFromPath(explicit_save_path);
  ASSERT_TRUE(latest_persisted.success);
  EXPECT_EQ(latest_persisted.data, latest_snapshot->data);
  std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> clean_snapshot;
  runtime_->emulator->ExportPRGNVRAM(
      true,
      kiwi::base::BindOnce(
          [](std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot>* output,
             std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot) {
            *output = std::move(snapshot);
          },
          &clean_snapshot));
  EXPECT_FALSE(clean_snapshot);

  std::optional<std::string> sha1 = kiwi::nes::CalculateINESRomSha1Hex(rom);
  ASSERT_TRUE(sha1);
  std::optional<kiwi::base::FilePath> automatic_path =
      battery_save::GetPath(runtime_->profile_path, *sha1);
  ASSERT_TRUE(automatic_path);
  EXPECT_FALSE(kiwi::base::PathExists(*automatic_path));
}

TEST_F(NESRuntimeBatterySaveTest, FlushesCurrentROMBeforeSwitching) {
  const kiwi::nes::Bytes first_rom = MakeWritingBatteryROM(0x20);
  const kiwi::base::FilePath first_rom_path = WriteROM(first_rom, "first.nes");
  const kiwi::base::FilePath explicit_save_path =
      runtime_->profile_path.Append(FILE_PATH_LITERAL("first.sav"));
  ASSERT_TRUE(battery_save::WriteToPath(explicit_save_path,
                                        kiwi::nes::Bytes(kPRGNVRAMSize)));
  ASSERT_TRUE(LoadROM(first_rom_path, explicit_save_path));
  runtime_->emulator->RunOneFrame();

  std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> expected_snapshot;
  runtime_->emulator->ExportPRGNVRAM(
      false,
      kiwi::base::BindOnce(
          [](std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot>* output,
             std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot) {
            *output = std::move(snapshot);
          },
          &expected_snapshot));
  ASSERT_TRUE(expected_snapshot);

  const kiwi::base::FilePath second_rom_path =
      WriteROM(MakeWritingBatteryROM(0x40), "second.nes");
  bool callback_called = false;
  bool loaded = false;
  runtime_->LoadROM(second_rom_path,
                    kiwi::base::BindOnce(
                        [](bool* callback_called, bool* loaded, bool result) {
                          *callback_called = true;
                          *loaded = result;
                        },
                        &callback_called, &loaded));

  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);
  io_task_runner_->RunNextTask();
  EXPECT_FALSE(callback_called);
  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);
  io_task_runner_->RunNextTask();
  EXPECT_FALSE(callback_called);

  battery_save::ReadResult persisted =
      battery_save::ReadFromPath(explicit_save_path);
  ASSERT_TRUE(persisted.success);
  EXPECT_EQ(persisted.data, expected_snapshot->data);

  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);
  io_task_runner_->RunNextTask();
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(loaded);
}

TEST_F(NESRuntimeBatterySaveTest, RetainsDirtyDataAfterWriteFailureAndRetries) {
  const kiwi::nes::Bytes rom = MakeWritingBatteryROM();
  const kiwi::base::FilePath rom_path = WriteROM(rom, "failure.nes");
  const kiwi::base::FilePath parent_path =
      runtime_->profile_path.Append(FILE_PATH_LITERAL("External"));
  const kiwi::base::FilePath save_path =
      parent_path.Append(FILE_PATH_LITERAL("failure.sav"));
  ASSERT_TRUE(
      battery_save::WriteToPath(save_path, kiwi::nes::Bytes(kPRGNVRAMSize)));
  ASSERT_TRUE(LoadROM(rom_path, save_path));
  runtime_->emulator->RunOneFrame();

  ASSERT_TRUE(kiwi::base::DeletePathRecursively(parent_path));
  ASSERT_TRUE(kiwi::base::WriteFile(parent_path, "x", 1));
  EXPECT_FALSE(FlushBatterySave());

  std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> dirty_snapshot;
  runtime_->emulator->ExportPRGNVRAM(
      true,
      kiwi::base::BindOnce(
          [](std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot>* output,
             std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot) {
            *output = std::move(snapshot);
          },
          &dirty_snapshot));
  ASSERT_TRUE(dirty_snapshot);

  ASSERT_TRUE(kiwi::base::DeletePathRecursively(parent_path));
  ASSERT_TRUE(kiwi::base::CreateDirectory(parent_path));
  EXPECT_TRUE(FlushBatterySave());
  std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> clean_snapshot;
  runtime_->emulator->ExportPRGNVRAM(
      true,
      kiwi::base::BindOnce(
          [](std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot>* output,
             std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot) {
            *output = std::move(snapshot);
          },
          &clean_snapshot));
  EXPECT_FALSE(clean_snapshot);
}

TEST_F(NESRuntimeBatterySaveTest, BacksUpInvalidAutomaticSaveBeforeRetry) {
  const kiwi::nes::Bytes rom = MakeWritingBatteryROM();
  const kiwi::base::FilePath rom_path = WriteROM(rom, "invalid.nes");
  std::optional<std::string> sha1 = kiwi::nes::CalculateINESRomSha1Hex(rom);
  ASSERT_TRUE(sha1);
  std::optional<kiwi::base::FilePath> save_path =
      battery_save::GetPath(runtime_->profile_path, *sha1);
  ASSERT_TRUE(save_path);
  ASSERT_TRUE(battery_save::WriteToPath(*save_path, kiwi::nes::Bytes{0x5a}));

  EXPECT_TRUE(LoadROM(rom_path));
  const kiwi::base::FilePath backup_path = kiwi::base::FilePath::FromUTF8Unsafe(
      save_path->AsUTF8Unsafe() + ".invalid.bak");
  EXPECT_TRUE(kiwi::base::PathExists(backup_path));
  battery_save::ReadResult backup = battery_save::ReadFromPath(backup_path);
  ASSERT_TRUE(backup.success);
  EXPECT_EQ(backup.data, kiwi::nes::Bytes({0x5a}));
}

TEST_F(NESRuntimeBatterySaveTest, PeriodicallyFlushesDirtyDataAndStopsCleanly) {
  const kiwi::nes::Bytes rom = MakeWritingBatteryROM();
  const kiwi::base::FilePath rom_path = WriteROM(rom, "periodic.nes");
  ASSERT_TRUE(LoadROM(rom_path));
  runtime_->emulator->RunOneFrame();

  runtime_->StartBatterySave(kiwi::base::Milliseconds(30000));
  ASSERT_EQ(timer_task_runner_->pending_task_count(), 1u);
  timer_task_runner_->RunNextTask();
  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);
  io_task_runner_->RunNextTask();
  EXPECT_EQ(timer_task_runner_->pending_task_count(), 1u);

  std::optional<std::string> sha1 = kiwi::nes::CalculateINESRomSha1Hex(rom);
  ASSERT_TRUE(sha1);
  battery_save::ReadResult persisted =
      battery_save::Read(runtime_->profile_path, *sha1);
  ASSERT_TRUE(persisted.success);
  ASSERT_TRUE(persisted.exists);
  EXPECT_EQ(persisted.data.size(), kPRGNVRAMSize);

  runtime_->StopBatterySave();
  timer_task_runner_->RunNextTask();
  EXPECT_EQ(timer_task_runner_->pending_task_count(), 0u);
  EXPECT_EQ(io_task_runner_->pending_task_count(), 0u);
}

TEST_F(NESRuntimeBatterySaveTest, FlushesDirtyDataBeforeUnload) {
  const kiwi::nes::Bytes rom = MakeWritingBatteryROM();
  const kiwi::base::FilePath rom_path = WriteROM(rom, "unload.nes");
  const kiwi::base::FilePath save_path =
      runtime_->profile_path.Append(FILE_PATH_LITERAL("unload.sav"));
  ASSERT_TRUE(
      battery_save::WriteToPath(save_path, kiwi::nes::Bytes(kPRGNVRAMSize)));
  ASSERT_TRUE(LoadROM(rom_path, save_path));
  runtime_->emulator->RunOneFrame();

  std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> expected_snapshot;
  runtime_->emulator->ExportPRGNVRAM(
      false,
      kiwi::base::BindOnce(
          [](std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot>* output,
             std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot) {
            *output = std::move(snapshot);
          },
          &expected_snapshot));
  ASSERT_TRUE(expected_snapshot);

  kiwi::base::RunLoop run_loop;
  bool callback_called = false;
  bool unloaded = false;
  runtime_->UnloadROM(kiwi::base::BindOnce(
      [](bool* callback_called, bool* unloaded,
         kiwi::base::RepeatingClosure quit, bool success) {
        *callback_called = true;
        *unloaded = success;
        quit.Run();
      },
      &callback_called, &unloaded, run_loop.QuitClosure()));
  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);
  io_task_runner_->RunNextTask();
  run_loop.Run();

  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(unloaded);
  EXPECT_EQ(runtime_->emulator->GetRunningState(),
            kiwi::nes::Emulator::RunningState::kStopped);
  battery_save::ReadResult persisted = battery_save::ReadFromPath(save_path);
  ASSERT_TRUE(persisted.success);
  EXPECT_EQ(persisted.data, expected_snapshot->data);
}

}  // namespace
