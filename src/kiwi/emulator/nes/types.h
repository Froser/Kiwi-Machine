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

#ifndef NES_TYPES_H_
#define NES_TYPES_H_

#include <stdint.h>
#include <iomanip>

#include "emulator/types.h"

namespace kiwi {
namespace nes {
using Register = int32_t;
using Bit = unsigned short;
using Byte = emu::Byte;
using Bytes = emu::Bytes;
using Color = emu::Color;
using Colors = emu::Colors;
using Sample = emu::Sample;
using Word = uint16_t;
using MapperId = uint16_t;
using Address = Word;
using ControllerButton = emu::ControllerButton;
struct Point {
  int x;
  int y;
};

// A device is an interface which can read from and write to an address.
class Device {
 public:
  Device() = default;
  virtual ~Device() = default;
  virtual Byte Read(Address address) = 0;
  virtual void Write(Address address, Byte value) = 0;
};

// Hex helper
template <int w>
struct TypeMatcher;

template <>
struct TypeMatcher<8> {
  using Type = uint8_t;
};
template <>
struct TypeMatcher<16> {
  using Type = uint16_t;
};

template <int w>
struct Hex {
  static constexpr int W = w;
  typename TypeMatcher<w>::Type hex;
};

template <int w>
std::ostream& operator<<(std::ostream& s, const Hex<w>& h) {
  return s << std::hex << std::setfill('0') << std::setw(Hex<w>::W / 4)
           << static_cast<int>(h.hex) << std::dec;
}
}  // namespace nes
}  // namespace kiwi

#endif  // NES_TYPES_H_
