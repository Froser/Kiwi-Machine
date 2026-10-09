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

#include "utility/switch/paths.h"

namespace kiwi::switch_platform {

kiwi::base::FilePath GetApplicationDirectory() {
  return kiwi::base::FilePath::FromUTF8Unsafe("sdmc:/switch/KiwiMachine");
}

kiwi::base::FilePath GetResourceDirectory() {
  return kiwi::base::FilePath::FromUTF8Unsafe("romfs:/");
}

kiwi::base::FilePath GetUserDataDirectory() {
  return GetApplicationDirectory().Append(FILE_PATH_LITERAL("userdata"));
}

}  // namespace kiwi::switch_platform
