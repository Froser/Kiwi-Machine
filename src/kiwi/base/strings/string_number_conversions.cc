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

#include <stdlib.h>
#include <charconv>

#include "base/check.h"

namespace kiwi::base {

std::string NumberToString(int value) {
  return std::to_string(value);
}

std::string NumberToString(unsigned int value) {
  return std::to_string(value);
}

std::string NumberToString(unsigned long value) {
  return std::to_string(value);
}

std::string NumberToString(int64_t value) {
  return std::to_string(value);
}

std::string NumberToString(uint64_t value) {
  return std::to_string(value);
}

bool StringToUint(StringPiece input, uint32_t* output) {
  DCHECK(output);
  return std::from_chars(input.data(), input.data() + input.size(), *output)
             .ec == std::errc();
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

bool HexStringToInt(StringPiece input, int32_t* output) {
  try {
    DCHECK(output);
    *output = std::stoi(std::string(input), nullptr, 16);
    return true;
  } catch (...) {
    return false;
  }
}

bool HexStringToUInt(StringPiece input, uint32_t* output) {
  try {
    DCHECK(output);
    *output = std::stoul(std::string(input), nullptr, 16);
    return true;
  } catch (...) {
    return false;
  }
}

bool StringToDouble(StringPiece input, double* output) {
  if (input.empty())
    return false;

  char* end;
  errno = 0;
  *output = strtod(input.data(), &end);
  return (end == input.data() + input.size()) && (errno != ERANGE) &&
         (end != input.data());
}

std::string HexEncode(const void* bytes, size_t size) {
  static const char kHexChars[] = "0123456789ABCDEF";

  // Each input byte creates two output hex characters.
  std::string ret(size * 2, '\0');

  for (size_t i = 0; i < size; ++i) {
    char b = reinterpret_cast<const char*>(bytes)[i];
    ret[(i * 2)] = kHexChars[(b >> 4) & 0xf];
    ret[(i * 2) + 1] = kHexChars[b & 0xf];
  }
  return ret;
}

std::string HexEncode(std::span<const uint8_t> bytes) {
  return HexEncode(bytes.data(), bytes.size());
}

}  // namespace kiwi::base
