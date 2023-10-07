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
#include <map>
#include <string>

#include "debug/debug_port.h"
#include "models/nes_config.h"

using NESRuntimeID = size_t;
class NESRuntime {
 public:
  struct Data {
    enum {
      MaxSaveStates = 10,
      MaxAutoSaveStates = 10,
    };

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

    using GetThumbnailCallback = kiwi::base::RepeatingCallback<
        kiwi::nes::IODevices::RenderDevice::Buffer()>;
    void StartAutoSave(kiwi::base::TimeDelta delta,
                       GetThumbnailCallback thumbnail);
    void StopAutoSave();

    kiwi::base::FilePath profile_path;

   private:
    kiwi::base::RepeatingClosure CreateAutoSaveClosure(
        kiwi::base::TimeDelta delta,
        GetThumbnailCallback thumbnail);
    void TriggerDelayedAutoSave(kiwi::base::TimeDelta delta,
                                GetThumbnailCallback thumbnail);
    bool auto_save_started_ = false;
  };

 private:
  NESRuntime();
  ~NESRuntime();

 public:
  Data* GetDataById(NESRuntimeID id);
  NESRuntimeID CreateData(const std::string& name);
  scoped_refptr<kiwi::base::SequencedTaskRunner> task_runner() {
    return task_runner_;
  }

 private:
  void CreateProfileIfNotExist(Data* data,
                               const kiwi::base::FilePath& profile_path);

 public:
  static NESRuntime* GetInstance();

 private:
  scoped_refptr<kiwi::base::SequencedTaskRunner> task_runner_;
};

#endif  // MODELS_NES_RUNTIME_H_