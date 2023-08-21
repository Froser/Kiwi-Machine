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

#include "base/strings/string_number_conversions.h"

#include <charconv>

#include "base/check.h"

namespace kiwi::base {

std::string NumberToString(int value) {
  return std::to_string(value);
}

bool StringToUint64(StringPiece input, uint64_t* output) {
  DCHECK(output);
  return std::from_chars(input.data(), input.data() + input.size(), *output)
             .ec == std::errc();
}

bool HexStringToUInt64(StringPiece input, uint64_t* output) {
  try {
    DCHECK(output);
    *output = std::stoull(std::string(input), nullptr, 16);
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace kiwi::base
