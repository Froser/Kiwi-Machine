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

namespace {

// Returns the number of bytes of the UTF-8 code point starting at |lead|.
// Falls back to 1 for invalid lead bytes so iteration always advances.
size_t Utf8SequenceLength(unsigned char lead) {
  if (lead < 0x80)
    return 1;
  if ((lead & 0xE0) == 0xC0)
    return 2;
  if ((lead & 0xF0) == 0xE0)
    return 3;
  if ((lead & 0xF8) == 0xF0)
    return 4;
  return 1;
}

// Splits a UTF-8 string into a list of code points (each kept as its raw byte
// sequence). ASCII bytes are lower-cased so matching stays case-insensitive;
// multi-byte code points (e.g. CJK) are preserved as-is.
std::vector<std::string> SplitIntoCodePoints(const std::string& str) {
  std::vector<std::string> code_points;
  size_t i = 0;
  while (i < str.size()) {
    size_t len = Utf8SequenceLength(static_cast<unsigned char>(str[i]));
    // Clamp to the remaining bytes to avoid reading past the end on malformed
    // input.
    if (i + len > str.size())
      len = str.size() - i;
    std::string cp = str.substr(i, len);
    if (len == 1)
      cp = kiwi::base::ToLowerASCII(cp);
    code_points.push_back(std::move(cp));
    i += len;
  }
  return code_points;
}

}  // namespace

bool HasString(const std::string& s1, const std::string& s2) {
  if (s2.empty()) {
    return true;
  }

  // Match by UTF-8 code points instead of raw bytes, so that multi-byte
  // characters (Chinese, Japanese, etc.) are compared as whole characters and
  // never match across character boundaries.
  std::vector<std::string> src = SplitIntoCodePoints(s1);
  std::vector<std::string> test = SplitIntoCodePoints(s2);

  size_t pos = 0;
  for (const std::string& cp : test) {
    bool found = false;
    for (; pos < src.size(); ++pos) {
      if (src[pos] == cp) {
        ++pos;
        found = true;
        break;
      }
    }
    if (!found)
      return false;
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