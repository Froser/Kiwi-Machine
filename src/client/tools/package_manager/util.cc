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

#include "util.h"
#include "base/strings/string_util.h"
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"

#include "../third_party/nlohmann_json/json.hpp"

namespace {

bool ReadCurrentFileFromZip(unzFile file, std::vector<uint8_t>& data) {
  unzOpenCurrentFile(file);
  unz_file_info fi;
  unzGetCurrentFileInfo(file, &fi, nullptr, 0, nullptr, 0, nullptr, 0);
  data.resize(fi.uncompressed_size);
  bool read = unzReadCurrentFile(file, data.data(), data.size()) == data.size();
  unzCloseCurrentFile(file);
  return read;
}
}  // namespace

bool IsPackageExtension(const std::string& filename) {
  kiwi::base::FilePath file_path =
      kiwi::base::FilePath::FromUTF8Unsafe(filename);
  return kiwi::base::ToLowerASCII(file_path.Extension()) ==
         FILE_PATH_LITERAL(".zip");
}

ROMS ReadPackageFromFile(const kiwi::base::FilePath& path) {
  static const std::vector<ROM> g_no_result;
  std::vector<ROM> result;
  unzFile file = unzOpen(path.AsUTF8Unsafe().c_str());
  if (file) {
    int err = unzLocateFile(file, "manifest.json", false);
    if (err != UNZ_OK) {
      unzClose(file);
      return g_no_result;
    }

    std::vector<uint8_t> manifest_data;
    if (!ReadCurrentFileFromZip(file, manifest_data)) {
      unzClose(file);
      return g_no_result;
    }

    nlohmann::json manifest_json = nlohmann::json::parse(manifest_data.data());
    if (manifest_json.contains("titles")) {
      const auto& titles = manifest_json.at("titles");
      for (const auto& rom_version : titles.items()) {
        ROM rom{0};
        strncpy(rom.name, rom_version.key().c_str(), rom.MAX);
        for (const auto& title : rom_version.value().items()) {
          if (title.key() == "zh") {
            std::string zh = title.value();
            strncpy(rom.zh, zh.c_str(), ROM::MAX);
          } else if (title.key() == "zh-hint") {
            std::string zh_hint = title.value();
            strncpy(rom.zh_hint, zh_hint.c_str(), ROM::MAX);
          } else if (title.key() == "ja") {
            std::string jp = title.value();
            strncpy(rom.ja, jp.c_str(), ROM::MAX);
          } else if (title.key() == "ja-hint") {
            std::string jp_hint = title.value();
            strncpy(rom.ja_hint, jp_hint.c_str(), ROM::MAX);
          }
        }
        result.push_back(rom);
      }
    } else {
      unzClose(file);
      return g_no_result;
    }

    unzClose(file);
  }

  // Sort results.
  std::sort(result.begin(), result.end(), [](const ROM& lhs, const ROM& rhs) {
    if (kiwi::base::CompareCaseInsensitiveASCII("default", lhs.name) == 0)
      return true;

    return strcmp(lhs.name, rhs.name) < 0;
  });
  return result;
}