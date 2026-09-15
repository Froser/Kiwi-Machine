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
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/task/sequenced_task_runner.h"
#include "build/kiwi_defines.h"
#include "debug/debug_port.h"
#include "models/nes_config.h"

using NESRuntimeID = size_t;
class NESRuntime {
 public:
  struct Data {
    enum {
      MaxSaveStates = 10,
      MaxAutoSaveStates = 50,
    };

    Data();
    explicit Data(scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner,
                  scoped_refptr<kiwi::base::SequencedTaskRunner>
                      timer_task_runner = nullptr);

    union ControllerMapping {
      int mapping[8];
      struct {
        int A;
        int B;
        int Select;
        int Start;
        int Up;
        int Down;
        int Left;
        int Right;
      };

      NESRuntime::Data::ControllerMapping& operator=(
          const NESRuntime::Data::ControllerMapping& rhs) {
        memcpy(this, &rhs, sizeof(rhs));
        return *this;
      }
    };

    struct JoystickMapping {
      void* which = nullptr;
      ControllerMapping mapping;
    };

    ControllerMapping keyboard_mappings[2] = {0};
    JoystickMapping joystick_mappings[2] = {nullptr, {0}};
    scoped_refptr<kiwi::nes::Emulator> emulator;
    std::unique_ptr<DebugPort> debug_port;

    void LoadROM(
        const kiwi::base::FilePath& rom_path,
        kiwi::nes::Emulator::LoadCallback callback,
        const std::optional<kiwi::base::FilePath>& save_path = std::nullopt);
    void LoadROM(kiwi::nes::Bytes rom_data,
                 kiwi::nes::Emulator::LoadCallback callback,
                 const kiwi::nes::Emulator::LoadOptions& options = {});
    void UnloadROM(kiwi::nes::Emulator::LoadCallback callback);
    void FlushBatterySave(kiwi::nes::Emulator::LoadCallback callback);

    void SaveState(int crc32,
                   int slot,
                   const kiwi::nes::Bytes& saved_state,
                   const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail,
                   kiwi::base::OnceCallback<void(bool)> callback);

    struct StateResult {
      bool success;
      kiwi::nes::Bytes state_data;
      kiwi::nes::Bytes thumbnail_data;  // Thumbnail data with 3 components
      int slot_or_timestamp;
    };

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

    void GetState(
        int crc32,
        int slot,
        kiwi::base::OnceCallback<void(const StateResult&)> load_callback);

#if KIWI_WASM
    // Checks if a save state exists for the given CRC and slot.
    bool SaveStateExists(int crc32, int slot);

    // Reads the raw JPEG thumbnail data for the given save state.
    kiwi::nes::Bytes ReadSaveStateThumbnail(int crc32, int slot);

    // Deletes a save state for the given CRC and slot.
    bool DeleteSaveState(int crc32, int slot);
#endif

    using GetThumbnailCallback = kiwi::base::RepeatingCallback<
        const kiwi::nes::IODevices::RenderDevice::Buffer&()>;
    void StartAutoSave(kiwi::base::TimeDelta delta,
                       GetThumbnailCallback thumbnail);
    void StopAutoSave();
    void StartBatterySave(kiwi::base::TimeDelta delta);
    void StopBatterySave();
    scoped_refptr<kiwi::base::SequencedTaskRunner> GetIOTaskRunner();

    kiwi::base::FilePath profile_path;

   private:
    void LoadROMData(kiwi::nes::Bytes rom_data,
                     kiwi::nes::Emulator::LoadCallback callback,
                     const kiwi::nes::Emulator::LoadOptions& options,
                     const std::optional<kiwi::base::FilePath>& save_path);
    void PauseAndFlushCurrentROM(kiwi::nes::Emulator::LoadCallback callback);
    void StartBatterySaveFlush();
    void OnBatterySaveFlushed(bool success);
    void TriggerDelayedBatterySave(kiwi::base::TimeDelta delta,
                                   uint64_t timer_generation);
    void RunPeriodicBatterySave(kiwi::base::TimeDelta delta,
                                uint64_t timer_generation);
    void OnPeriodicBatterySaveFlushed(kiwi::base::TimeDelta delta,
                                      uint64_t timer_generation,
                                      bool success);

    kiwi::base::RepeatingClosure CreateAutoSaveClosure(
        kiwi::base::TimeDelta delta,
        GetThumbnailCallback thumbnail);
    void TriggerDelayedAutoSave(kiwi::base::TimeDelta delta,
                                GetThumbnailCallback thumbnail);
    bool auto_save_started_ = false;
    bool battery_save_started_ = false;
    bool battery_save_flush_in_progress_ = false;
    bool battery_save_flush_requested_ = false;
    uint64_t battery_save_timer_generation_ = 0;
    std::optional<kiwi::base::FilePath> current_battery_save_path_;
    std::vector<kiwi::nes::Emulator::LoadCallback>
        battery_save_flush_callbacks_;
    scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner_;
    scoped_refptr<kiwi::base::SequencedTaskRunner> timer_task_runner_;
  };

 private:
  NESRuntime();
  ~NESRuntime();

 public:
  Data* GetDataById(NESRuntimeID id);
  NESRuntimeID CreateData(const std::string& name);

 private:
  void CreateProfileIfNotExist(Data* data,
                               const kiwi::base::FilePath& profile_path);

 public:
  static NESRuntime* GetInstance();
};

#endif  // MODELS_NES_RUNTIME_H_
