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

#include "models/runtime.h"

#include <SDL.h>

#include <cstring>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_SWITCH)
#include "utility/switch/paths.h"
#endif

Runtime::ControllerMapping& Runtime::ControllerMapping::operator=(
    const ControllerMapping& rhs) {
  std::memcpy(this, &rhs, sizeof(rhs));
  return *this;
}

Runtime::Runtime(
    const kiwi::base::FilePath& profile_path,
    scoped_refptr<kiwi::base::SequencedTaskRunner> timer_task_runner)
    : profile_path_(profile_path),
      timer_task_runner_(std::move(timer_task_runner)) {
  if (!kiwi::base::PathExists(profile_path_) &&
      !kiwi::base::CreateDirectory(profile_path_)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Can't open or create profile directory at %s",
                 profile_path_.AsUTF8Unsafe().c_str());
  }
}

Runtime::~Runtime() {
  Shutdown();
}

void Runtime::StartBatterySave(kiwi::base::TimeDelta delta) {
  if (shutdown_ || battery_save_started_) {
    return;
  }

  battery_save_started_ = true;
  TriggerDelayedBatterySave(delta, ++battery_save_timer_generation_);
}

void Runtime::StopBatterySave() {
  battery_save_started_ = false;
  ++battery_save_timer_generation_;
}

void Runtime::StopPeriodicSaves() {
  StopBatterySave();
  OnStopPeriodicSaves();
}

void Runtime::Shutdown() {
  if (shutdown_) {
    return;
  }

  shutdown_ = true;
  StopPeriodicSaves();
  lifetime_.reset();
}

std::weak_ptr<int> Runtime::GetWeakLifetime() const {
  return lifetime_;
}

void Runtime::OnStopPeriodicSaves() {}

void Runtime::TriggerDelayedBatterySave(kiwi::base::TimeDelta delta,
                                        uint64_t timer_generation) {
  if (!battery_save_started_ ||
      timer_generation != battery_save_timer_generation_) {
    return;
  }

  scoped_refptr<kiwi::base::SequencedTaskRunner> task_runner =
      timer_task_runner_
          ? timer_task_runner_
          : kiwi::base::SingleThreadTaskRunner::GetCurrentDefault();
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  task_runner->PostDelayedTask(
      FROM_HERE,
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, Runtime* runtime,
             kiwi::base::TimeDelta delta, uint64_t timer_generation) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->RunPeriodicBatterySave(delta, timer_generation);
          },
          std::move(weak_lifetime), this, delta, timer_generation),
      delta);
}

void Runtime::RunPeriodicBatterySave(kiwi::base::TimeDelta delta,
                                     uint64_t timer_generation) {
  if (!battery_save_started_ ||
      timer_generation != battery_save_timer_generation_) {
    return;
  }
  if (!emulator()->IsRunning()) {
    TriggerDelayedBatterySave(delta, timer_generation);
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  FlushBatterySave(kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, Runtime* runtime,
         kiwi::base::TimeDelta delta, uint64_t timer_generation, bool success) {
        if (weak_lifetime.expired()) {
          return;
        }
        runtime->OnPeriodicBatterySaveFlushed(delta, timer_generation, success);
      },
      std::move(weak_lifetime), this, delta, timer_generation));
}

void Runtime::OnPeriodicBatterySaveFlushed(kiwi::base::TimeDelta delta,
                                           uint64_t timer_generation,
                                           bool success) {
  if (!success) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Could not periodically save battery-backed data.");
  }
  TriggerDelayedBatterySave(delta, timer_generation);
}

const Runtime::ControllerMapping& Runtime::keyboard_mapping(int player) const {
  SDL_assert(player == 0 || player == 1);
  return keyboard_mappings_[player];
}

void Runtime::set_keyboard_mapping(int player,
                                   const ControllerMapping& mapping) {
  SDL_assert(player == 0 || player == 1);
  keyboard_mappings_[player] = mapping;
}

const Runtime::JoystickMapping& Runtime::joystick_mapping(int player) const {
  SDL_assert(player == 0 || player == 1);
  return joystick_mappings_[player];
}

Runtime::JoystickMapping& Runtime::joystick_mapping(int player) {
  SDL_assert(player == 0 || player == 1);
  return joystick_mappings_[player];
}

void Runtime::set_joystick_mapping(int player, const JoystickMapping& mapping) {
  SDL_assert(player == 0 || player == 1);
  joystick_mappings_[player] = mapping;
}

kiwi::base::FilePath Runtime::GetProfilePath(const std::string& name) {
#if BUILDFLAG(IS_WASM)
  return kiwi::base::FilePath::FromUTF8Unsafe("/persistent")
      .Append(kiwi::base::FilePath::FromUTF8Unsafe(name));
#elif BUILDFLAG(IS_SWITCH)
  return kiwi::switch_platform::GetUserDataDirectory().Append(
      kiwi::base::FilePath::FromUTF8Unsafe(name));
#else
  char* pref_path = SDL_GetPrefPath("Kiwi", "KiwiMachine");
  kiwi::base::FilePath profile_path =
      kiwi::base::FilePath::FromUTF8Unsafe(pref_path).Append(
          kiwi::base::FilePath::FromUTF8Unsafe(name));
  SDL_free(pref_path);
  return profile_path;
#endif
}
