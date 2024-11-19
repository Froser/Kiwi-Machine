// Copyright (C) 2024 Yisi Yu
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

#ifndef UTIL_H_
#define UTIL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"

bool IsPackageExtension(const std::string& filename);

struct ROM {
  static constexpr size_t MAX = 128;

  char name[MAX];

  // Title
  char zh[MAX];
  char zh_hint[MAX];
  char ja[MAX];
  char ja_hint[MAX];
};

using ROMS = std::vector<ROM>;

[[nodiscard]] ROMS ReadPackageFromFile(const kiwi::base::FilePath& path);

#endif  // UTIL_H_