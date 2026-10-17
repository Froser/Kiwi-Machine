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

#include "utility/runtime_util.h"

#include <SDL.h>

#include <utility>

#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "models/gba/runtime.h"

namespace runtime_util {
namespace {

constexpr int kBatterySaveIntervalMilliseconds = 30000;
#if !BUILDFLAG(IS_WASM)
constexpr int kAutoStateSaveIntervalMilliseconds = 5000;
#endif

void ConfigureDefaultKeyboardMappings(Runtime* runtime) {
  Runtime::ControllerMapping player_one = {SDLK_j, SDLK_k, SDLK_l, SDLK_RETURN,
                                           SDLK_w, SDLK_s, SDLK_a, SDLK_d,
                                           SDLK_u, SDLK_o};
  Runtime::ControllerMapping player_two = {
      SDLK_DELETE, SDLK_END,  SDLK_PAGEDOWN, SDLK_HOME,    SDLK_UP,
      SDLK_DOWN,   SDLK_LEFT, SDLK_RIGHT,    SDLK_UNKNOWN, SDLK_UNKNOWN};
  runtime->set_keyboard_mapping(0, player_one);
  runtime->set_keyboard_mapping(1, player_two);
}

}  // namespace

kiwi::emu::Emulator::Type GetEmulationSystemForROMPath(
    const kiwi::base::FilePath& rom_path) {
  const std::string extension =
      kiwi::base::FilePath(rom_path.FinalExtension()).AsUTF8Unsafe();
  return kiwi::base::CompareCaseInsensitiveASCII(extension, ".gba") == 0
             ? kiwi::emu::Emulator::Type::kGBA
             : kiwi::emu::Emulator::Type::kNES;
}

JoystickMappings CopyJoystickMappings(const Runtime* runtime) {
  JoystickMappings mappings = {};
  if (!runtime) {
    return mappings;
  }

  for (std::size_t player = 0; player < mappings.size(); ++player) {
    mappings[player] = runtime->joystick_mapping(static_cast<int>(player));
  }
  return mappings;
}

std::unique_ptr<Runtime> CreateRuntime(kiwi::emu::Emulator::Type system,
                                       const kiwi::base::FilePath& profile_path,
                                       CreateRuntimeOptions options) {
  std::unique_ptr<Runtime> runtime;
  if (system == kiwi::emu::Emulator::Type::kGBA) {
    runtime = std::make_unique<GBARuntime>(profile_path);
  } else {
    auto nes_runtime = std::make_unique<NESRuntime>(profile_path);
    if (options.breakpoint_callback) {
      kiwi::nes::Emulator* emulator = nes_runtime->emulator()->AsNESEmulator();
      emulator->SetDebugPort(nes_runtime->debug_port());
      nes_runtime->debug_port()->set_on_breakpoint_callback(
          std::move(options.breakpoint_callback));
    }
    runtime = std::move(nes_runtime);
  }

  runtime->emulator()->PowerOn();
  ConfigureDefaultKeyboardMappings(runtime.get());
  for (std::size_t player = 0; player < options.joystick_mappings.size();
       ++player) {
    runtime->set_joystick_mapping(static_cast<int>(player),
                                  options.joystick_mappings[player]);
  }
  runtime->emulator()->SetVolume(options.volume);
  return runtime;
}

void ShutdownRuntime(Runtime* runtime) {
  if (!runtime) {
    return;
  }
  runtime->Shutdown();
  runtime->emulator()->PowerOff();
}

void StartPeriodicSaves(Runtime* runtime,
                        NESRuntime::GetThumbnailCallback thumbnail_callback) {
  if (!runtime) {
    return;
  }

  runtime->StartBatterySave(
      kiwi::base::Milliseconds(kBatterySaveIntervalMilliseconds));

#if !BUILDFLAG(IS_WASM)
  if (runtime->emulator()->GetType() == kiwi::emu::Emulator::Type::kNES) {
    static_cast<NESRuntime*>(runtime)->StartAutoStateSave(
        kiwi::base::Milliseconds(kAutoStateSaveIntervalMilliseconds),
        std::move(thumbnail_callback));
  }
#else
  static_cast<void>(thumbnail_callback);
#endif
}

void StopPeriodicSaves(Runtime* runtime) {
  if (runtime) {
    runtime->StopPeriodicSaves();
  }
}

}  // namespace runtime_util
