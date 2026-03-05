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