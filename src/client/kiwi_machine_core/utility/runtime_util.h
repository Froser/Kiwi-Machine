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

#ifndef UTILITY_RUNTIME_UTIL_H_
#define UTILITY_RUNTIME_UTIL_H_

#include <array>
#include <cstddef>
#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "emulator/emulator.h"
#include "models/nes/runtime.h"
#include "models/runtime.h"

namespace runtime_util {

constexpr std::size_t kPlayerCount = 2;
using JoystickMappings = std::array<Runtime::JoystickMapping, kPlayerCount>;

struct CreateRuntimeOptions {
  float volume = 1.f;
  JoystickMappings joystick_mappings = {};
  kiwi::base::RepeatingClosure breakpoint_callback;
};

kiwi::emu::Emulator::Type GetEmulationSystemForROMPath(
    const kiwi::base::FilePath& rom_path);

JoystickMappings CopyJoystickMappings(const Runtime* runtime);

std::unique_ptr<Runtime> CreateRuntime(kiwi::emu::Emulator::Type system,
                                       const kiwi::base::FilePath& profile_path,
                                       CreateRuntimeOptions options = {});

void ShutdownRuntime(Runtime* runtime);

void StartPeriodicSaves(Runtime* runtime,
                        NESRuntime::GetThumbnailCallback thumbnail_callback);
void StopPeriodicSaves(Runtime* runtime);

}  // namespace runtime_util

#endif  // UTILITY_RUNTIME_UTIL_H_
