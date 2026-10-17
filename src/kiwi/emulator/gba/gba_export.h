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

#ifndef GBA_GBA_EXPORT_H_
#define GBA_GBA_EXPORT_H_

#if !defined(KIWI_STATIC_LIBRARY)
#if defined(WIN32)

#if defined(KIWI_IMPL)
#define GBA_EXPORT __declspec(dllexport)
#else
#define GBA_EXPORT __declspec(dllimport)
#endif  // defined(KIWI_IMPL)

#else  // defined(WIN32)
#if defined(KIWI_IMPL)
#define GBA_EXPORT __attribute__((visibility("default")))
#else
#define GBA_EXPORT
#endif  // defined(KIWI_IMPL)
#endif

#else
#define GBA_EXPORT
#endif

#endif  // GBA_GBA_EXPORT_H_
