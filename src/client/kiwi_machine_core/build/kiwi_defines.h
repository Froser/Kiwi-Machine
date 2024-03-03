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

#include <SDL_platform.h>

#if (defined(__IPHONEOS__) && __IPHONEOS__)
#define KIWI_IOS 1
#else
#define KIWI_IOS 0
#endif

#if defined(ANDROID)
#define KIWI_ANDROID 1
#else
#define KIWI_ANDROID 0
#endif

#if defined(__EMSCRIPTEN__)
#define KIWI_WASM 1
#else
#define KIWI_WASM 0
#endif

#if KIWI_IOS || KIWI_ANDROID
#define KIWI_MOBILE 1
#else
#define KIWI_MOBILE 0
#endif

#if KIWI_WASM
#define ENABLE_DEBUG_ROMS 0
#define ENABLE_EXPORT_ROMS 0
#define DISABLE_CHINESE_FONT 1   // To save space, Wasm removes Chinese font
#define DISABLE_JAPANESE_FONT 1  // To save space, Wasm removes Japanese font
// To save space, Wasm removes all basic sound effects, see
// resources/audio/wasm_ignore.json
#define DISABLE_SOUND_EFFECTS 1

#else
#define ENABLE_DEBUG_ROMS 1
#define ENABLE_EXPORT_ROMS 1
#define DISABLE_CHINESE_FONT 0
#define DISABLE_JAPANESE_FONT 0
#define DISABLE_SOUND_EFFECTS 0
#endif

#endif  // BUILD_KIWI_DEFINES_H_