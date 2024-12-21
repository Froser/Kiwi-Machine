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

#ifndef WORKSPACE_H_
#define WORKSPACE_H_

#include "../third_party/nlohmann_json/json.hpp"
#include "base/files/file_path.h"

struct Workspace {
  struct Manifest {
    std::string nes_roms_dir;
    std::string zipped_nes_dir;
    std::string nes_boxarts_dir;
  };
  char workspace_dir[1024] = {};

  Workspace();
  bool ReadFromManifest(const kiwi::base::FilePath& manifest_file);

  kiwi::base::FilePath GetOutPath();
  kiwi::base::FilePath GetTestPath();
  kiwi::base::FilePath GetNESRomsPath();
  kiwi::base::FilePath GetZippedPath();
  kiwi::base::FilePath GetPackageOutputPath();
  kiwi::base::FilePath GetNESBoxartsPath();

 private:
  kiwi::base::FilePath workspace_path_;
  Manifest manifest_;
};

Workspace& GetWorkspace();

#endif  // WORKSPACE_H_