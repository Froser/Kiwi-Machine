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

#ifndef UTILITY_SWITCH_PATHS_INTERNAL_H_
#define UTILITY_SWITCH_PATHS_INTERNAL_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"

namespace kiwi::switch_platform::internal {

using AccountId = std::array<uint64_t, 2>;

inline kiwi::base::FilePath ResolveUserDataDirectory(
    const kiwi::base::FilePath& application_directory,
    const std::optional<AccountId>& account_id) {
  kiwi::base::FilePath user_data_directory =
      application_directory.Append(FILE_PATH_LITERAL("userdata"));
  if (!account_id)
    return user_data_directory;

  std::string directory_name = kiwi::base::StringPrintf(
      "%016llx%016llx", static_cast<unsigned long long>((*account_id)[0]),
      static_cast<unsigned long long>((*account_id)[1]));
  return user_data_directory.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(directory_name));
}

}  // namespace kiwi::switch_platform::internal

#endif  // UTILITY_SWITCH_PATHS_INTERNAL_H_
