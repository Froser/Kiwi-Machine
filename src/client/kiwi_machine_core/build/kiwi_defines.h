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

#ifndef BUILD_KIWI_DEFINES_H_
#define BUILD_KIWI_DEFINES_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_SWITCH)
#define BUILDFLAG_INTERNAL_ENABLE_HD_TEXTURE() (0)
#else
#define BUILDFLAG_INTERNAL_ENABLE_HD_TEXTURE() (1)
#endif

#if BUILDFLAG(IS_WASM)
#define BUILDFLAG_INTERNAL_ENABLE_DEBUG_ROMS() (0)
#define DISABLE_CHINESE_FONT 1   // To save space, Wasm removes Chinese font
#define DISABLE_JAPANESE_FONT 1  // To save space, Wasm removes Japanese font
// To save space, Wasm removes all basic sound effects, see
// resources/audio/wasm_ignore.json
#define DISABLE_SOUND_EFFECTS 1

#else
#define BUILDFLAG_INTERNAL_ENABLE_DEBUG_ROMS() (1)
#define DISABLE_CHINESE_FONT 0
#define DISABLE_JAPANESE_FONT 0
#define DISABLE_SOUND_EFFECTS 0
#endif

#endif  // BUILD_KIWI_DEFINES_H_
