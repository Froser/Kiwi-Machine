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

#include "preset_roms/preset_roms.h"
#include "utility/zip_reader.h"

#include <gflags/gflags.h>
#include <vector>

DEFINE_string(
    test_pak,
    "",
    "Specifies a package for testing. It will be added to the side menu");

namespace preset_roms {
std::vector<Package*> GetPresetRomsPackages();

std::vector<Package*> GetPresetOrTestRomsPackages() {
#if defined(KIWI_USE_EXTERNAL_PAK)
  if (!FLAGS_test_pak.empty()) {
    static std::vector<Package*> leaky_package = {CreatePackageFromFile(
        kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_test_pak))};
    return leaky_package;
  } else {
    return preset_roms::GetPresetRomsPackages();
  }
#else
  return preset_roms::GetPresetRomsPackages();
#endif
}

}  // namespace preset_roms