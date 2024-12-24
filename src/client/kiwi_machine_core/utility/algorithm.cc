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
  std::string src_string = kiwi::base::ToLowerASCII(s1);
  std::string test_string = kiwi::base::ToLowerASCII(s2);
  for (int i = 0; i < src_string.size(); ++i) {
    char c = src_string[i];
    auto pos = test_string.find(c);
    if (pos != std::string::npos) {
      if (test_string.length() == 1)
        return i == test_string.length() - 1;

      test_string = test_string.substr(pos + 1);
    } else {
      return false;
    }
  }
  return true;
}