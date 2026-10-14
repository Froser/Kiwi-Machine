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

#include <cstdint>
#include <vector>

namespace kiwi {
namespace gba {

using Byte = uint8_t;
using Bytes = std::vector<Byte>;
using Color = uint32_t;
using Colors = std::vector<Color>;
using Sample = int16_t;

constexpr int kFrameWidth = 240;
constexpr int kFrameHeight = 160;

enum class ControllerButton {
  kA,
  kB,
  kSelect,
  kStart,
  kRight,
  kLeft,
  kUp,
  kDown,
  kR,
  kL,

  kMax,
};

}  // namespace gba
}  // namespace kiwi

#endif  // GBA_TYPES_H_
