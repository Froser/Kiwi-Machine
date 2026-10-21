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

#ifndef GBA_TYPES_H_
#define GBA_TYPES_H_

#include "emulator/types.h"

namespace kiwi {
namespace gba {

using Byte = emu::Byte;
using Bytes = emu::Bytes;
// mGBA's default 32-bit color_t layout: 0xAABBGGRR.
using Color = emu::Color;
using Colors = emu::Colors;
using Sample = emu::Sample;
using ControllerButton = emu::ControllerButton;

constexpr int kFrameWidth = 240;
constexpr int kFrameHeight = 160;

}  // namespace gba
}  // namespace kiwi

#endif  // GBA_TYPES_H_
