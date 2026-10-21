// Copyright (C) 2023 Yisi Yu
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

#ifndef MODELS_NES_RUNTIME_H_
#define MODELS_NES_RUNTIME_H_

#include <kiwi_nes.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/kiwi_defines.h"
#include "debug/debug_port.h"
#include "models/runtime.h"

class NESRuntime : public Runtime {
 public:
  enum {
    MaxSaveStates = Runtime::MaxSaveStates,
    MaxAutoSaveStates = Runtime::MaxAutoSaveStates,
  };
  using StateResult = Runtime::StateResult;
  using GetThumbnailCallback = Runtime::GetThumbnailCallback;

  explicit NESRuntime(
      const kiwi::base::FilePath& profile_path,
      scoped_refptr<kiwi::nes::Emulator> emulator = nullptr,
      scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner = nullptr,
      scoped_refptr<kiwi::base::SequencedTaskRunner> timer_task_runner =
          nullptr);
  ~NESRuntime() override;

  void LoadROM(const kiwi::base::FilePath& rom_path,
               kiwi::emu::Emulator::LoadCallback callback) override;
  void LoadROM(kiwi::emu::Bytes rom_data,
               kiwi::emu::Emulator::LoadCallback callback) override;
  void UnloadROM(kiwi::emu::Emulator::LoadCallback callback) override;
  void FlushBatterySave(kiwi::emu::Emulator::LoadCallback callback) override;
  scoped_refptr<kiwi::emu::Emulator> emulator() override;
  void PersistState(int slot,
                    const kiwi::emu::Bytes& state_data,
                    const kiwi::emu::Colors& thumbnail,
                    kiwi::base::OnceCallback<void(bool)> callback) override;
  void GetSavedStatesCount(
      kiwi::base::OnceCallback<void(int)> callback) override;
  void GetState(int slot, StateResultCallback callback) override;
  void GetAutoSavedStatesCount(
      kiwi::base::OnceCallback<void(int)> callback) override;
  void GetAutoSavedState(int slot, StateResultCallback callback) override;
  void GetAutoSavedStateByTimestamp(uint64_t timestamp,
                                    StateResultCallback callback) override;
  void DeleteAutoSavedStatesNewerThan(
      uint64_t timestamp,
      kiwi::base::OnceCallback<void(bool)> callback) override;

  void LoadROM(const kiwi::base::FilePath& rom_path,
               kiwi::emu::Emulator::LoadCallback callback,
               const std::optional<kiwi::base::FilePath>& save_path);
  void LoadROM(kiwi::emu::Bytes rom_data,
               kiwi::emu::Emulator::LoadCallback callback,
               const kiwi::nes::Emulator::LoadOptions& options);

  DebugPort* debug_port() { return debug_port_.get(); }
  const DebugPort* debug_port() const { return debug_port_.get(); }

  void SaveState(int crc32,
                 int slot,
                 const kiwi::nes::Bytes& saved_state,
                 const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail,
                 kiwi::base::OnceCallback<void(bool)> callback);

  void GetSavedStatesCount(int crc32,
                           kiwi::base::OnceCallback<void(int)> callback);
  void GetAutoSavedStatesCount(int crc32,
                               kiwi::base::OnceCallback<void(int)> callback);
  void GetAutoSavedState(
      int crc32,
      int slot,
      kiwi::base::OnceCallback<void(const StateResult&)> load_callback);
  void GetAutoSavedStateByTimestamp(
      int crc32,
      uint64_t timestamp,
      kiwi::base::OnceCallback<void(const StateResult&)> load_callback);
  void DeleteAutoSavedStatesNewerThan(
      int crc32,
      uint64_t timestamp,
      kiwi::base::OnceCallback<void(bool)> callback);
  void GetState(
      int crc32,
      int slot,
      kiwi::base::OnceCallback<void(const StateResult&)> load_callback);

#if BUILDFLAG(IS_WASM)
  bool SaveStateExists(int crc32, int slot);
  kiwi::nes::Bytes ReadSaveStateThumbnail(int crc32, int slot);
  bool DeleteSaveState(int crc32, int slot);
#endif

  void StartAutoStateSave(kiwi::base::TimeDelta delta,
                          GetThumbnailCallback thumbnail) override;
  void StopAutoStateSave() override;
  scoped_refptr<kiwi::base::SequencedTaskRunner> GetIOTaskRunner();

 private:
  void OnStopPeriodicSaves() override;
  kiwi::nes::Emulator* nes_emulator();
  const kiwi::nes::Emulator* nes_emulator() const;
  void LoadROMData(kiwi::emu::Bytes rom_data,
                   kiwi::emu::Emulator::LoadCallback callback,
                   const kiwi::nes::Emulator::LoadOptions& options,
                   const std::optional<kiwi::base::FilePath>& save_path);
  void PauseAndFlushCurrentROM(kiwi::emu::Emulator::LoadCallback callback);
  void StartBatterySaveFlush();
  void OnBatterySaveFlushed(bool success);
  void TriggerDelayedAutoSave(kiwi::base::TimeDelta delta,
                              GetThumbnailCallback thumbnail,
                              uint64_t timer_generation);
  void RunAutoStateSave(kiwi::base::TimeDelta delta,
                        GetThumbnailCallback thumbnail,
                        uint64_t timer_generation);
  void OnAutoStateCaptured(kiwi::base::TimeDelta delta,
                           GetThumbnailCallback thumbnail,
                           uint64_t timer_generation,
                           int crc32,
                           kiwi::nes::Bytes data);
  void OnAutoStateWritten(kiwi::base::TimeDelta delta,
                          GetThumbnailCallback thumbnail,
                          uint64_t timer_generation,
                          bool success);

  scoped_refptr<kiwi::nes::Emulator> emulator_;
  std::unique_ptr<DebugPort> debug_port_;
  bool auto_save_started_ = false;
  uint64_t auto_save_timer_generation_ = 0;
  bool battery_save_flush_in_progress_ = false;
  bool battery_save_flush_requested_ = false;
  std::optional<kiwi::base::FilePath> current_battery_save_path_;
  std::vector<kiwi::emu::Emulator::LoadCallback> battery_save_flush_callbacks_;
  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner_;
};

#endif  // MODELS_NES_RUNTIME_H_
