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

#ifndef MODELS_RUNTIME_H_
#define MODELS_RUNTIME_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "emulator/emulator.h"

class Runtime {
 public:
  union ControllerMapping {
    int mapping[10];
    struct {
      int A;
      int B;
      int Select;
      int Start;
      int Up;
      int Down;
      int Left;
      int Right;
      int L;  // For GBA only.
      int R;  // For GBA only.
    };

    ControllerMapping& operator=(const ControllerMapping& rhs);
  };

  struct JoystickMapping {
    void* which = nullptr;
    ControllerMapping mapping = {0};
  };

  explicit Runtime(const kiwi::base::FilePath& profile_path,
                   scoped_refptr<kiwi::base::SequencedTaskRunner>
                       timer_task_runner = nullptr);
  virtual ~Runtime();

  Runtime(const Runtime&) = delete;
  Runtime& operator=(const Runtime&) = delete;

  virtual void LoadROM(const kiwi::base::FilePath& rom_path,
                       kiwi::emu::Emulator::LoadCallback callback) = 0;
  virtual void LoadROM(kiwi::emu::Bytes rom_data,
                       kiwi::emu::Emulator::LoadCallback callback) = 0;
  virtual void UnloadROM(kiwi::emu::Emulator::LoadCallback callback) = 0;
  virtual void FlushBatterySave(kiwi::emu::Emulator::LoadCallback callback) = 0;
  virtual scoped_refptr<kiwi::emu::Emulator> emulator() = 0;

  void StartBatterySave(kiwi::base::TimeDelta delta);
  void StopBatterySave();
  void StopPeriodicSaves();
  void Shutdown();

  const kiwi::base::FilePath& profile_path() const { return profile_path_; }

  const ControllerMapping& keyboard_mapping(int player) const;
  void set_keyboard_mapping(int player, const ControllerMapping& mapping);
  const JoystickMapping& joystick_mapping(int player) const;
  JoystickMapping& joystick_mapping(int player);
  void set_joystick_mapping(int player, const JoystickMapping& mapping);

  static kiwi::base::FilePath GetProfilePath(const std::string& name);

 protected:
  std::weak_ptr<int> GetWeakLifetime() const;
  virtual void OnStopPeriodicSaves();

 private:
  void TriggerDelayedBatterySave(kiwi::base::TimeDelta delta,
                                 uint64_t timer_generation);
  void RunPeriodicBatterySave(kiwi::base::TimeDelta delta,
                              uint64_t timer_generation);
  void OnPeriodicBatterySaveFlushed(kiwi::base::TimeDelta delta,
                                    uint64_t timer_generation,
                                    bool success);

  kiwi::base::FilePath profile_path_;
  ControllerMapping keyboard_mappings_[2] = {0};
  JoystickMapping joystick_mappings_[2] = {};
  scoped_refptr<kiwi::base::SequencedTaskRunner> timer_task_runner_;
  bool battery_save_started_ = false;
  bool shutdown_ = false;
  uint64_t battery_save_timer_generation_ = 0;
  std::shared_ptr<int> lifetime_ = std::make_shared<int>(0);
};

#endif  // MODELS_RUNTIME_H_
