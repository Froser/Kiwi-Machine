// Copyright (C) 2025 Yisi Yu
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

#include "utility/android/asset.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include "third_party/SDL2/src/core/android/SDL_android.h"

std::vector<kiwi::base::FilePath> GetAssets(
    const kiwi::base::FilePath& directory) {
  AAssetManager* assetManager = Android_JNI_Get_AssetManager();
  if (assetManager == NULL) {
    return std::vector<kiwi::base::FilePath>();
  }

  std::vector<kiwi::base::FilePath> result;
  AAssetDir* assetDir =
      AAssetManager_openDir(assetManager, directory.AsUTF8Unsafe().c_str());
  const char* filename;
  filename = AAssetDir_getNextFileName(assetDir);
  while (filename) {
    result.push_back(
        directory.Append(kiwi::base::FilePath::FromUTF8Unsafe(filename)));
    filename = AAssetDir_getNextFileName(assetDir);
  }
  AAssetDir_close(assetDir);
  return result;
}

std::vector<kiwi::base::FilePath> GetAssets() {
  return GetAssets(kiwi::base::FilePath());
}
