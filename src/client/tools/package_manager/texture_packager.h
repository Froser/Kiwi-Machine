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

#ifndef TEXTURE_PACKAGER_H_
#define TEXTURE_PACKAGER_H_

#include <vector>

#include "base/files/file_path.h"

// Packages every direct child directory as a standalone Mesen HD texture PAK.
// Files are stored relative to the child directory and manifest.json is
// generated at the PAK root.
std::vector<kiwi::base::FilePath> PackMesenHDTextures(
    const kiwi::base::FilePath& textures_dir,
    const kiwi::base::FilePath& output_dir);

#endif  // TEXTURE_PACKAGER_H_
