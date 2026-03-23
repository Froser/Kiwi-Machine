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

#include "utility/algorithm.h"

#include <kiwi_nes.h>

namespace {}

bool HasString(const std::string& s1, const std::string& s2) {
  if (s2.empty()) {
    return true;
  }

  std::string src_string = kiwi::base::ToLowerASCII(s1);
  std::string test_string = kiwi::base::ToLowerASCII(s2);
  
  size_t pos = 0;
  for (char c : test_string) {
    pos = src_string.find(c, pos);
    if (pos == std::string::npos) {
      return false;
    }
    ++pos;
  }
  
  return true;
}

std::string Base64Encode(const kiwi::nes::Byte* data, size_t len) {
  static const char* chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string result;
  result.reserve(((len + 2) / 3) * 4);
  for (size_t i = 0; i < len; i += 3) {
    uint32_t n = static_cast<uint32_t>(data[i]) << 16;
    if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
    if (i + 2 < len) n |= data[i + 2];
    result += chars[(n >> 18) & 0x3F];
    result += chars[(n >> 12) & 0x3F];
    result += (i + 1 < len) ? chars[(n >> 6) & 0x3F] : '=';
    result += (i + 2 < len) ? chars[n & 0x3F] : '=';
  }
  return result;
}