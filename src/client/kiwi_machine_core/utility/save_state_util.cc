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

#include "emulator/gba/emulator.h"

namespace save_state_util {
namespace {

void Complete(StateOperationCallback callback, int value, bool success) {
  std::move(callback).Run(value, success);
}

void CaptureState(Runtime* runtime,
                  kiwi::emu::Emulator::SaveStateCallback callback) {
  if (runtime->emulator()->GetType() == kiwi::emu::Emulator::Type::kGBA) {
    runtime->emulator()->AsGBAEmulator()->SaveState(std::move(callback));
  } else {
    runtime->emulator()->AsNESEmulator()->SaveState(std::move(callback));
  }
}

void ApplyStateToEmulator(Runtime* runtime,
                          const kiwi::emu::Bytes& state_data,
                          kiwi::emu::Emulator::LoadCallback callback) {
  if (runtime->emulator()->GetType() == kiwi::emu::Emulator::Type::kGBA) {
    runtime->emulator()->AsGBAEmulator()->LoadState(state_data,
                                                    std::move(callback));
  } else {
    runtime->emulator()->AsNESEmulator()->LoadState(state_data,
                                                    std::move(callback));
  }
}

void OnStateApplied(Runtime* runtime,
                    int slot_or_timestamp,
                    bool discard_newer_states,
                    StateOperationCallback callback,
                    bool success) {
  if (success && discard_newer_states) {
    runtime->DeleteAutoSavedStatesNewerThan(
        static_cast<uint64_t>(slot_or_timestamp),
        kiwi::base::BindOnce([](bool deleted) {
          if (!deleted) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to delete newer auto saved states.");
          }
        }));
  }
  Complete(std::move(callback), slot_or_timestamp, success);
}

void ApplyState(Runtime* runtime,
                bool discard_newer_states,
                kiwi::base::OnceClosure prepare_to_load,
                StateOperationCallback callback,
                const Runtime::StateResult& state_result) {
  if (!state_result.success || state_result.state_data.empty()) {
    Complete(std::move(callback), state_result.slot_or_timestamp, false);
    return;
  }

  if (prepare_to_load) {
    std::move(prepare_to_load).Run();
  }
  ApplyStateToEmulator(
      runtime, state_result.state_data,
      discard_newer_states
          ? kiwi::base::BindOnce(&OnStateApplied, runtime,
                                 state_result.slot_or_timestamp, true,
                                 std::move(callback))
          : kiwi::base::BindOnce(&Complete, std::move(callback),
                                 state_result.slot_or_timestamp));
}

void OnStateCaptured(Runtime* runtime,
                     int slot,
                     Runtime::GetThumbnailCallback thumbnail_callback,
                     StateOperationCallback callback,
                     kiwi::emu::Bytes state_data) {
  if (state_data.empty() || !thumbnail_callback) {
    Complete(std::move(callback), slot, false);
    return;
  }

  runtime->PersistState(
      slot, state_data, thumbnail_callback.Run(),
      kiwi::base::BindOnce(&Complete, std::move(callback), slot));
}

}  // namespace

bool CanSaveOrLoadState(Runtime* runtime) {
  return runtime && runtime->emulator()->HasROM();
}

void SaveState(Runtime* runtime,
               int slot,
               Runtime::GetThumbnailCallback thumbnail_callback,
               StateOperationCallback callback) {
  SDL_assert(slot >= 0 && slot < Runtime::MaxSaveStates);
  if (!CanSaveOrLoadState(runtime)) {
    Complete(std::move(callback), slot, false);
    return;
  }

  CaptureState(runtime, kiwi::base::BindOnce(&OnStateCaptured, runtime, slot,
                                             std::move(thumbnail_callback),
                                             std::move(callback)));
}

void LoadState(Runtime* runtime,
               int slot,
               kiwi::base::OnceClosure prepare_to_load,
               StateOperationCallback callback) {
  SDL_assert(slot >= 0 && slot < Runtime::MaxSaveStates);
  if (!CanSaveOrLoadState(runtime)) {
    Complete(std::move(callback), slot, false);
    return;
  }

  runtime->GetState(slot, kiwi::base::BindOnce(&ApplyState, runtime, false,
                                               std::move(prepare_to_load),
                                               std::move(callback)));
}

void LoadAutoSavedState(Runtime* runtime,
                        uint64_t timestamp,
                        bool discard_newer_states,
                        kiwi::base::OnceClosure prepare_to_load,
                        StateOperationCallback callback) {
  if (!CanSaveOrLoadState(runtime)) {
    Complete(std::move(callback), static_cast<int>(timestamp), false);
    return;
  }

  runtime->GetAutoSavedStateByTimestamp(
      timestamp,
      kiwi::base::BindOnce(&ApplyState, runtime, discard_newer_states,
                           std::move(prepare_to_load), std::move(callback)));
}

}  // namespace save_state_util
