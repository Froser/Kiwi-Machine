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

#ifndef NES_EXPORT_H_
#define NES_EXPORT_H_

#if !defined(KIWI_STATIC_LIBRARY)
#if defined(WIN32)

#if defined(KIWI_IMPL)
#define NES_EXPORT __declspec(dllexport)
#else
#define NES_EXPORT __declspec(dllimport)
#endif  // defined(KIWI_IMPL)

#else  // defined(WIN32)
#if defined(KIWI_IMPL)
#define NES_EXPORT __attribute__((visibility("default")))
#else
#define NES_EXPORT
#endif  // defined(KIWI_IMPL)
#endif

#else  //
#define NES_EXPORT
#endif

#endif  // NES_EXPORT_H_
