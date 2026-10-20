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

#include "models/gba/runtime.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <filesystem>
#include <memory>
#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_executor.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"
#include "utility/save_state_util.h"

namespace {

void WriteWord(kiwi::gba::Bytes& data, size_t offset, uint32_t value) {
  for (size_t byte = 0; byte < sizeof(value); ++byte) {
    data[offset + byte] = static_cast<kiwi::gba::Byte>(value >> (byte * 8));
  }
}

kiwi::gba::Bytes MakeTestROM(bool writes_sram = false) {
  kiwi::gba::Bytes rom(0xc0);
  WriteWord(rom, 0, 0xeafffffe);
  if (writes_sram) {
    WriteWord(rom, 0, 0xea000006);
    WriteWord(rom, 0x20, 0xe59f0008);
    WriteWord(rom, 0x24, 0xe3a0103c);
    WriteWord(rom, 0x28, 0xe5c01000);
    WriteWord(rom, 0x2c, 0xeafffffe);
    WriteWord(rom, 0x30, 0x0e000123);
    constexpr char kSaveMarker[] = "SRAM_V110";
    std::copy(kSaveMarker, kSaveMarker + sizeof(kSaveMarker) - 1,
              rom.begin() + 0x40);
  }

  constexpr char kTitle[] = "KIWI TEST";
  constexpr char kGameCode[] = "KTSE";
  std::copy(kTitle, kTitle + sizeof(kTitle) - 1, rom.begin() + 0xa0);
  std::copy(kGameCode, kGameCode + sizeof(kGameCode) - 1, rom.begin() + 0xac);
  rom[0xb2] = 0x96;
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

class GBARuntimeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    task_executor_ = std::make_unique<kiwi::base::SingleThreadTaskExecutor>();
    io_task_runner_ = kiwi::base::MakeRefCounted<DeferredTaskRunner>();
    timer_task_runner_ = kiwi::base::MakeRefCounted<DeferredTaskRunner>();

    const std::string directory_name =
        "kiwi_gba_runtime_" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    profile_path_ = kiwi::base::FilePath::FromUTF8Unsafe(
        (std::filesystem::temp_directory_path() / directory_name).string());
    runtime_ = std::make_unique<GBARuntime>(
        profile_path_, kiwi::gba::CreateEmulatorForTesting(), io_task_runner_,
        timer_task_runner_);
    runtime_->emulator()->PowerOn();
  }

  void TearDown() override {
    runtime_->StopPeriodicSaves();
    timer_task_runner_->RunUntilIdle();
    io_task_runner_->RunUntilIdle();
    runtime_->emulator()->PowerOff();
    runtime_.reset();
    EXPECT_TRUE(kiwi::base::DeletePathRecursively(profile_path_));
    io_task_runner_.reset();
    timer_task_runner_.reset();
    task_executor_.reset();
  }

  bool LoadROM(kiwi::gba::Bytes rom) {
    bool callback_called = false;
    bool success = false;
    runtime_->LoadROM(std::move(rom), kiwi::base::BindOnce(
                                          [](bool* callback_called,
                                             bool* success, bool result) {
                                            *callback_called = true;
                                            *success = result;
                                          },
                                          &callback_called, &success));
    EXPECT_FALSE(callback_called);
    io_task_runner_->RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return success;
  }

  std::unique_ptr<kiwi::base::SingleThreadTaskExecutor> task_executor_;
  scoped_refptr<DeferredTaskRunner> io_task_runner_;
  scoped_refptr<DeferredTaskRunner> timer_task_runner_;
  kiwi::base::FilePath profile_path_;
  std::unique_ptr<GBARuntime> runtime_;
};

TEST_F(GBARuntimeTest, InvalidROMRestoresPreviousRunningSession) {
  ASSERT_TRUE(LoadROM(MakeTestROM()));
  ASSERT_TRUE(runtime_->emulator()->IsRunning());

  EXPECT_FALSE(LoadROM(kiwi::gba::Bytes{0xea}));

  EXPECT_TRUE(runtime_->emulator()->IsRunning());
  const kiwi::gba::RomData* rom_data =
      runtime_->emulator()->AsGBAEmulator()->GetRomData();
  ASSERT_NE(rom_data, nullptr);
  EXPECT_EQ(rom_data->title, "KIWI TEST");
}

TEST_F(GBARuntimeTest, SaveFailureDuringUnloadRestoresRunningSession) {
  ASSERT_TRUE(LoadROM(MakeTestROM(true)));
  runtime_->emulator()->RunOneFrame();

  const kiwi::base::FilePath gba_path =
      profile_path_.Append(FILE_PATH_LITERAL("gba"));
  ASSERT_TRUE(kiwi::base::WriteFile(gba_path, "x", 1));

  bool callback_called = false;
  bool unloaded = true;
  runtime_->UnloadROM(kiwi::base::BindOnce(
      [](bool* callback_called, bool* unloaded, bool success) {
        *callback_called = true;
        *unloaded = success;
      },
      &callback_called, &unloaded));
  EXPECT_FALSE(callback_called);
  io_task_runner_->RunUntilIdle();

  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(unloaded);
  EXPECT_TRUE(runtime_->emulator()->IsRunning());
  EXPECT_TRUE(runtime_->emulator()->HasROM());
}

TEST_F(GBARuntimeTest, PeriodicallyFlushesBatterySave) {
  ASSERT_TRUE(LoadROM(MakeTestROM(true)));
  runtime_->emulator()->RunOneFrame();

  runtime_->StartBatterySave(kiwi::base::Milliseconds(30000));
  ASSERT_EQ(timer_task_runner_->pending_task_count(), 1u);
  timer_task_runner_->RunNextTask();
  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);
  io_task_runner_->RunNextTask();

  const kiwi::base::FilePath saves_path =
      profile_path_.Append(FILE_PATH_LITERAL("gba"))
          .Append(FILE_PATH_LITERAL("saves"));
  kiwi::base::FileEnumerator saves(saves_path, false,
                                   kiwi::base::FileEnumerator::FILES);
  const kiwi::base::FilePath save_path = saves.Next();
  ASSERT_FALSE(save_path.empty());
  EXPECT_FALSE(kiwi::base::ReadFileToBytes(save_path)->empty());
  EXPECT_TRUE(saves.Next().empty());
}

TEST_F(GBARuntimeTest, SavesAndLoadsManualStateWithThumbnail) {
  ASSERT_TRUE(LoadROM(MakeTestROM()));
  runtime_->emulator()->RunOneFrame();

  kiwi::emu::Colors thumbnail(kiwi::gba::kFrameWidth * kiwi::gba::kFrameHeight,
                              0xff336699);
  bool save_callback_called = false;
  bool saved = false;
  save_state_util::SaveState(
      runtime_.get(), 2,
      kiwi::base::BindRepeating(
          [](const kiwi::emu::Colors* pixels) -> const kiwi::emu::Colors& {
            return *pixels;
          },
          &thumbnail),
      kiwi::base::BindOnce(
          [](bool* callback_called, bool* saved, int slot, bool success) {
            *callback_called = true;
            *saved = success;
            EXPECT_EQ(slot, 2);
          },
          &save_callback_called, &saved));
  EXPECT_FALSE(save_callback_called);
  io_task_runner_->RunNextTask();
  EXPECT_TRUE(save_callback_called);
  EXPECT_TRUE(saved);

  int count = -1;
  runtime_->GetSavedStatesCount(kiwi::base::BindOnce(
      [](int* output, int value) { *output = value; }, &count));
  io_task_runner_->RunNextTask();
  EXPECT_EQ(count, 1);

  Runtime::StateResult state;
  runtime_->GetState(
      2, kiwi::base::BindOnce(
             [](Runtime::StateResult* output,
                const Runtime::StateResult& value) { *output = value; },
             &state));
  io_task_runner_->RunNextTask();
  EXPECT_TRUE(state.success);
  EXPECT_FALSE(state.state_data.empty());
  EXPECT_EQ(state.slot_or_timestamp, 2);
  EXPECT_EQ(state.thumbnail_width, kiwi::gba::kFrameWidth);
  EXPECT_EQ(state.thumbnail_height, kiwi::gba::kFrameHeight);
  EXPECT_EQ(state.thumbnail_data.size(),
            static_cast<size_t>(kiwi::gba::kFrameWidth *
                                kiwi::gba::kFrameHeight * 4));

  bool prepared = false;
  bool load_callback_called = false;
  bool loaded = false;
  save_state_util::LoadState(
      runtime_.get(), 2,
      kiwi::base::BindOnce([](bool* value) { *value = true; }, &prepared),
      kiwi::base::BindOnce(
          [](bool* callback_called, bool* loaded, int slot, bool success) {
            *callback_called = true;
            *loaded = success;
            EXPECT_EQ(slot, 2);
          },
          &load_callback_called, &loaded));
  EXPECT_FALSE(load_callback_called);
  io_task_runner_->RunNextTask();
  EXPECT_TRUE(prepared);
  EXPECT_TRUE(load_callback_called);
  EXPECT_TRUE(loaded);
}

TEST_F(GBARuntimeTest, AutoSnapshotsAreTimestampedLimitedAndCleanable) {
  ASSERT_TRUE(LoadROM(MakeTestROM()));
  runtime_->emulator()->RunOneFrame();

  const kiwi::gba::RomData* rom_data =
      runtime_->emulator()->AsGBAEmulator()->GetRomData();
  ASSERT_NE(rom_data, nullptr);
  const kiwi::base::FilePath states_path =
      profile_path_.Append(FILE_PATH_LITERAL("gba"))
          .Append(FILE_PATH_LITERAL("States"))
          .Append(kiwi::base::FilePath::FromUTF8Unsafe(
              kiwi::base::StringPrintf("%08x", rom_data->crc32)));

  const kiwi::emu::Bytes seed_state = {1, 2, 3, 4};
  kiwi::emu::Colors thumbnail(kiwi::gba::kFrameWidth * kiwi::gba::kFrameHeight,
                              0xff224466);
  bool persisted = false;
  runtime_->PersistState(
      2, seed_state, thumbnail,
      kiwi::base::BindOnce(
          [](bool* output, bool success) { *output = success; }, &persisted));
  io_task_runner_->RunNextTask();
  ASSERT_TRUE(persisted);

  const kiwi::base::FilePath manual_path =
      states_path.Append(FILE_PATH_LITERAL("2"));
  std::optional<kiwi::emu::Bytes> encoded_thumbnail =
      kiwi::base::ReadFileToBytes(
          manual_path.Append(FILE_PATH_LITERAL("thumbnail")));
  ASSERT_TRUE(encoded_thumbnail);
  ASSERT_FALSE(encoded_thumbnail->empty());

  const kiwi::base::FilePath auto_saved_path =
      states_path.Append(FILE_PATH_LITERAL("AutoSaved"));
  for (int timestamp = 100; timestamp <= 150; ++timestamp) {
    const kiwi::base::FilePath snapshot_path = auto_saved_path.Append(
        kiwi::base::FilePath::FromUTF8Unsafe(std::to_string(timestamp)));
    ASSERT_TRUE(kiwi::base::CreateDirectory(snapshot_path));
    ASSERT_TRUE(kiwi::base::WriteFile(
        snapshot_path.Append(FILE_PATH_LITERAL("data")), seed_state));
    ASSERT_TRUE(kiwi::base::WriteFile(
        snapshot_path.Append(FILE_PATH_LITERAL("thumbnail")),
        *encoded_thumbnail));
  }

  runtime_->StartAutoStateSave(
      kiwi::base::Milliseconds(5000),
      kiwi::base::BindRepeating(
          [](const kiwi::emu::Colors* pixels) -> const kiwi::emu::Colors& {
            return *pixels;
          },
          &thumbnail));
  ASSERT_EQ(timer_task_runner_->pending_task_count(), 1u);
  timer_task_runner_->RunNextTask();
  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);
  io_task_runner_->RunNextTask();
  EXPECT_EQ(timer_task_runner_->pending_task_count(), 1u);

  int count = -1;
  runtime_->GetAutoSavedStatesCount(kiwi::base::BindOnce(
      [](int* output, int value) { *output = value; }, &count));
  io_task_runner_->RunNextTask();
  EXPECT_EQ(count, Runtime::MaxAutoSaveStates);

  Runtime::StateResult latest;
  runtime_->GetAutoSavedState(
      0, kiwi::base::BindOnce(
             [](Runtime::StateResult* output,
                const Runtime::StateResult& value) { *output = value; },
             &latest));
  io_task_runner_->RunNextTask();
  ASSERT_TRUE(latest.success);
  EXPECT_GT(latest.slot_or_timestamp, 150);

  Runtime::StateResult selected;
  runtime_->GetAutoSavedState(
      10, kiwi::base::BindOnce(
              [](Runtime::StateResult* output,
                 const Runtime::StateResult& value) { *output = value; },
              &selected));
  io_task_runner_->RunNextTask();
  ASSERT_TRUE(selected.success);
  EXPECT_EQ(selected.slot_or_timestamp, 141);

  Runtime::StateResult by_timestamp;
  runtime_->GetAutoSavedStateByTimestamp(
      selected.slot_or_timestamp,
      kiwi::base::BindOnce(
          [](Runtime::StateResult* output, const Runtime::StateResult& value) {
            *output = value;
          },
          &by_timestamp));
  io_task_runner_->RunNextTask();
  EXPECT_TRUE(by_timestamp.success);
  EXPECT_EQ(by_timestamp.slot_or_timestamp, selected.slot_or_timestamp);

  bool deleted = false;
  runtime_->DeleteAutoSavedStatesNewerThan(
      selected.slot_or_timestamp,
      kiwi::base::BindOnce(
          [](bool* output, bool success) { *output = success; }, &deleted));
  io_task_runner_->RunNextTask();
  EXPECT_TRUE(deleted);

  count = -1;
  runtime_->GetAutoSavedStatesCount(kiwi::base::BindOnce(
      [](int* output, int value) { *output = value; }, &count));
  io_task_runner_->RunNextTask();
  EXPECT_EQ(count, 40);

  runtime_->StopAutoStateSave();
  timer_task_runner_->RunUntilIdle();
}

TEST_F(GBARuntimeTest, ShutdownInvalidatesPendingLoadReply) {
  bool callback_called = false;
  runtime_->LoadROM(MakeTestROM(),
                    kiwi::base::BindOnce([](bool* callback_called,
                                            bool) { *callback_called = true; },
                                         &callback_called));
  ASSERT_EQ(io_task_runner_->pending_task_count(), 1u);
  runtime_->StartBatterySave(kiwi::base::Milliseconds(30000));
  ASSERT_EQ(timer_task_runner_->pending_task_count(), 1u);

  runtime_->Shutdown();
  runtime_->emulator()->PowerOff();
  runtime_.reset();
  io_task_runner_->RunUntilIdle();
  timer_task_runner_->RunUntilIdle();

  EXPECT_FALSE(callback_called);

  runtime_ = std::make_unique<GBARuntime>(profile_path_,
                                          kiwi::gba::CreateEmulatorForTesting(),
                                          io_task_runner_, timer_task_runner_);
  runtime_->emulator()->PowerOn();
}

}  // namespace
