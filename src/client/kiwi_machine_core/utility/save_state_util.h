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

#ifndef UTILITY_SAVE_STATE_UTIL_H_
#define UTILITY_SAVE_STATE_UTIL_H_

#include <cstdint>

#include "base/functional/callback.h"
#include "models/nes/runtime.h"

namespace save_state_util {

using StateOperationCallback = kiwi::base::OnceCallback<void(int, bool)>;

bool CanSaveOrLoadState(NESRuntime* runtime);

void SaveState(NESRuntime* runtime,
               int slot,
               NESRuntime::GetThumbnailCallback thumbnail_callback,
               StateOperationCallback callback);

void LoadState(NESRuntime* runtime,
               int slot,
               kiwi::base::OnceClosure prepare_to_load,
               StateOperationCallback callback);

void LoadAutoSavedState(NESRuntime* runtime,
                        uint64_t timestamp,
                        bool discard_newer_states,
                        kiwi::base::OnceClosure prepare_to_load,
                        StateOperationCallback callback);

}  // namespace save_state_util

#endif  // UTILITY_SAVE_STATE_UTIL_H_
