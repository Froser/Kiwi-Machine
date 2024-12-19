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

#include <gflags/gflags.h>
#include <vector>

#include "kiwi_flags.h"
#include "utility/zip_reader.h"

DEFINE_string(
    test_pak,
    "",
    "Specifies a package for testing. It will be added to the side menu");
DEFINE_string(test_rom, "", "Specifies a ROM's path to load");

namespace preset_roms {
std::vector<Package*> GetPresetRomsPackages();

std::vector<Package*> GetPresetOrTestRomsPackages() {
  // When a test rom is specified, it means we are in a headless mode.
  if (!FLAGS_test_rom.empty()) {
    return std::vector<Package*>();
  }

  if (!FLAGS_test_pak.empty()) {
    static std::vector<Package*> leaky_package = {CreatePackageFromFile(
        kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_test_pak))};
    return leaky_package;
  } else {
    return preset_roms::GetPresetRomsPackages();
  }
}

}  // namespace preset_roms