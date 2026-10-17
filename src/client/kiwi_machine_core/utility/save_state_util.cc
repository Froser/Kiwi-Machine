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

#include "utility/save_state_util.h"

#include <SDL.h>

#include <utility>

namespace save_state_util {
namespace {

void Complete(StateOperationCallback callback, int value, bool success) {
  std::move(callback).Run(value, success);
}

void OnStateApplied(NESRuntime* runtime,
                    int slot_or_timestamp,
                    bool discard_newer_states,
                    StateOperationCallback callback,
                    bool success) {
  if (success && discard_newer_states) {
    const kiwi::nes::RomData* rom_data =
        runtime->emulator()->AsNESEmulator()->GetRomData();
    if (rom_data) {
      runtime->DeleteAutoSavedStatesNewerThan(
          rom_data->crc, slot_or_timestamp,
          kiwi::base::BindOnce([](bool deleted) {
            if (!deleted) {
              SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                          "Failed to delete newer auto saved states.");
            }
          }));
    }
  }
  Complete(std::move(callback), slot_or_timestamp, success);
}

void ApplyState(NESRuntime* runtime,
                bool discard_newer_states,
                kiwi::base::OnceClosure prepare_to_load,
                StateOperationCallback callback,
                const NESRuntime::StateResult& state_result) {
  if (!state_result.success || state_result.state_data.empty()) {
    Complete(std::move(callback), state_result.slot_or_timestamp, false);
    return;
  }

  if (prepare_to_load) {
    std::move(prepare_to_load).Run();
  }
  runtime->emulator()->AsNESEmulator()->LoadState(
      state_result.state_data,
      kiwi::base::BindOnce(&OnStateApplied, runtime,
                           state_result.slot_or_timestamp,
                           discard_newer_states, std::move(callback)));
}

void OnStateCaptured(NESRuntime* runtime,
                     int slot,
                     NESRuntime::GetThumbnailCallback thumbnail_callback,
                     StateOperationCallback callback,
                     kiwi::nes::Bytes state_data) {
  const kiwi::nes::RomData* rom_data =
      runtime->emulator()->AsNESEmulator()->GetRomData();
  if (!rom_data || state_data.empty() || !thumbnail_callback) {
    Complete(std::move(callback), slot, false);
    return;
  }

  runtime->SaveState(
      rom_data->crc, slot, state_data, thumbnail_callback.Run(),
      kiwi::base::BindOnce(&Complete, std::move(callback), slot));
}

}  // namespace

bool CanSaveOrLoadState(NESRuntime* runtime) {
  return runtime &&
         runtime->emulator()->AsNESEmulator()->GetRunningState() !=
             kiwi::nes::Emulator::RunningState::kStopped;
}

void SaveState(NESRuntime* runtime,
               int slot,
               NESRuntime::GetThumbnailCallback thumbnail_callback,
               StateOperationCallback callback) {
  SDL_assert(slot >= 0 && slot < NESRuntime::MaxSaveStates);
  if (!CanSaveOrLoadState(runtime)) {
    Complete(std::move(callback), slot, false);
    return;
  }

  runtime->emulator()->AsNESEmulator()->SaveState(
      kiwi::base::BindOnce(&OnStateCaptured, runtime, slot,
                           std::move(thumbnail_callback), std::move(callback)));
}

void LoadState(NESRuntime* runtime,
               int slot,
               kiwi::base::OnceClosure prepare_to_load,
               StateOperationCallback callback) {
  SDL_assert(slot >= 0 && slot < NESRuntime::MaxSaveStates);
  if (!CanSaveOrLoadState(runtime)) {
    Complete(std::move(callback), slot, false);
    return;
  }

  const kiwi::nes::RomData* rom_data =
      runtime->emulator()->AsNESEmulator()->GetRomData();
  if (!rom_data) {
    Complete(std::move(callback), slot, false);
    return;
  }

  runtime->GetState(
      rom_data->crc, slot,
      kiwi::base::BindOnce(&ApplyState, runtime, false,
                           std::move(prepare_to_load), std::move(callback)));
}

void LoadAutoSavedState(NESRuntime* runtime,
                        uint64_t timestamp,
                        bool discard_newer_states,
                        kiwi::base::OnceClosure prepare_to_load,
                        StateOperationCallback callback) {
  if (!CanSaveOrLoadState(runtime)) {
    Complete(std::move(callback), static_cast<int>(timestamp), false);
    return;
  }

  const kiwi::nes::RomData* rom_data =
      runtime->emulator()->AsNESEmulator()->GetRomData();
  if (!rom_data) {
    Complete(std::move(callback), static_cast<int>(timestamp), false);
    return;
  }

  runtime->GetAutoSavedStateByTimestamp(
      rom_data->crc, timestamp,
      kiwi::base::BindOnce(&ApplyState, runtime, discard_newer_states,
                           std::move(prepare_to_load), std::move(callback)));
}

}  // namespace save_state_util
