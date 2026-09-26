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

#ifndef UTILITY_SWITCH_PATHS_H_
#define UTILITY_SWITCH_PATHS_H_

#include "base/files/file_path.h"

namespace kiwi::switch_platform {

kiwi::base::FilePath GetApplicationDirectory();
kiwi::base::FilePath GetResourceDirectory();
kiwi::base::FilePath GetUserDataDirectory();

}  // namespace kiwi::switch_platform

#endif  // UTILITY_SWITCH_PATHS_H_
