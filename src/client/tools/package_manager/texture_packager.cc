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

#include "texture_packager.h"

#include <algorithm>
#include <cstdio>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "../third_party/nlohmann_json/json.hpp"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "nes/components/mesen_hd_pack/hires_parser.h"
#include "third_party/zlib-1.3.2/contrib/minizip/zip.h"

namespace {

bool IsSameOrParentPath(const kiwi::base::FilePath& parent,
                        const kiwi::base::FilePath& path) {
  const std::vector<kiwi::base::FilePath::StringType> parent_components =
      parent.StripTrailingSeparators().GetComponents();
  const std::vector<kiwi::base::FilePath::StringType> path_components =
      path.StripTrailingSeparators().GetComponents();
  return parent_components.size() <= path_components.size() &&
         std::equal(parent_components.begin(), parent_components.end(),
                    path_components.begin());
}

bool WriteToZip(zipFile zip,
                const std::string& filename,
                const char* data,
                size_t size) {
  zip_fileinfo info{};
  if (zipOpenNewFileInZip(zip, filename.c_str(), &info, nullptr, 0, nullptr, 0,
                          nullptr, Z_DEFLATED,
                          Z_DEFAULT_COMPRESSION) != ZIP_OK) {
    return false;
  }

  const bool success = zipWriteInFileInZip(zip, data, size) == ZIP_OK;
  zipCloseFileInZip(zip);
  return success;
}

std::string GetArchivePath(const kiwi::base::FilePath& root,
                           const kiwi::base::FilePath& file) {
  const auto root_value = root.StripTrailingSeparators().value();
  const auto& file_value = file.value();
  if (file_value.size() <= root_value.size() ||
      file_value.compare(0, root_value.size(), root_value) != 0) {
    return {};
  }

  std::string path =
      kiwi::base::FilePath(file_value.substr(root_value.size() + 1))
          .AsUTF8Unsafe();
  std::replace(path.begin(), path.end(), '\\', '/');
  return path;
}

bool CollectSupportedRoms(const kiwi::base::FilePath& pack_dir,
                          std::set<std::string>* supported_roms) {
  kiwi::base::FileEnumerator definitions(
      pack_dir, true, kiwi::base::FileEnumerator::FILES,
      FILE_PATH_LITERAL("hires.txt"),
      kiwi::base::FileEnumerator::FolderSearchPolicy::ALL);
  bool found_definition = false;
  for (kiwi::base::FilePath path = definitions.Next(); !path.empty();
       path = definitions.Next()) {
    found_definition = true;
    std::optional<std::vector<uint8_t>> contents =
        kiwi::base::ReadFileToBytes(path);
    if (!contents) {
      std::fprintf(stderr, "Failed to read Mesen HD Pack definition: %s\n",
                   path.AsUTF8Unsafe().c_str());
      return false;
    }

    const char* data = contents->empty()
                           ? ""
                           : reinterpret_cast<const char*>(contents->data());
    kiwi::nes::mesen_hd_pack::HdPackData pack_data;
    std::vector<kiwi::nes::mesen_hd_pack::ParseError> errors;
    kiwi::nes::mesen_hd_pack::HiresParser parser;
    if (!parser.Parse(std::string_view(data, contents->size()), &pack_data,
                      &errors)) {
      for (const auto& error : errors) {
        std::fprintf(stderr, "%s:%zu: %s\n", path.AsUTF8Unsafe().c_str(),
                     error.line, error.message.c_str());
      }
      return false;
    }
    supported_roms->insert(pack_data.supported_rom_sha1s.begin(),
                           pack_data.supported_rom_sha1s.end());
  }

  if (!found_definition || supported_roms->empty()) {
    std::fprintf(stderr, "Mesen HD Pack has no supported ROM: %s\n",
                 pack_dir.AsUTF8Unsafe().c_str());
    return false;
  }
  return true;
}

kiwi::base::FilePath PackMesenHDTexture(
    const kiwi::base::FilePath& pack_dir,
    const kiwi::base::FilePath& output_dir) {
  std::set<std::string> supported_roms;
  if (!CollectSupportedRoms(pack_dir, &supported_roms)) {
    return {};
  }

  const kiwi::base::FilePath output_path = output_dir.Append(
      pack_dir.BaseName().value() + FILE_PATH_LITERAL(".pak"));
  zipFile zip =
      zipOpen(output_path.AsUTF8Unsafe().c_str(), APPEND_STATUS_CREATE);
  if (!zip) {
    std::fprintf(stderr, "Failed to create texture package: %s\n",
                 output_path.AsUTF8Unsafe().c_str());
    return {};
  }

  bool success = true;
  std::vector<kiwi::base::FilePath> pack_files;
  kiwi::base::FileEnumerator files(pack_dir, true,
                                   kiwi::base::FileEnumerator::FILES);
  for (kiwi::base::FilePath path = files.Next(); !path.empty();
       path = files.Next()) {
    pack_files.push_back(std::move(path));
  }
  std::sort(pack_files.begin(), pack_files.end());

  for (const kiwi::base::FilePath& path : pack_files) {
    const std::string archive_path = GetArchivePath(pack_dir, path);
    if (archive_path.empty() || archive_path == "manifest.json") {
      continue;
    }

    std::optional<std::vector<uint8_t>> contents =
        kiwi::base::ReadFileToBytes(path);
    if (!contents) {
      success = false;
      break;
    }
    const char* data = contents->empty()
                           ? ""
                           : reinterpret_cast<const char*>(contents->data());
    if (!WriteToZip(zip, archive_path, data, contents->size())) {
      success = false;
      break;
    }
  }

  if (success) {
    nlohmann::json manifest = {
        {"type", "mesen"},
        {"roms", std::vector<std::string>(supported_roms.begin(),
                                          supported_roms.end())},
    };
    const std::string contents = manifest.dump(2);
    success =
        WriteToZip(zip, "manifest.json", contents.data(), contents.size());
  }

  zipClose(zip, nullptr);
  if (!success) {
    std::remove(output_path.AsUTF8Unsafe().c_str());
    return {};
  }
  return output_path;
}

}  // namespace

std::vector<kiwi::base::FilePath> PackMesenHDTextures(
    const kiwi::base::FilePath& textures_dir,
    const kiwi::base::FilePath& output_dir) {
  std::vector<kiwi::base::FilePath> results;
  if (!kiwi::base::DirectoryExists(textures_dir)) {
    std::fprintf(stderr, "Texture directory does not exist: %s\n",
                 textures_dir.AsUTF8Unsafe().c_str());
    return results;
  }
  if (IsSameOrParentPath(textures_dir, output_dir) ||
      IsSameOrParentPath(output_dir, textures_dir)) {
    std::fprintf(stderr,
                 "Texture input and output directories must not overlap.\n");
    return results;
  }
  if (!kiwi::base::DeletePathRecursively(output_dir)) {
    std::fprintf(stderr, "Failed to clean texture output directory: %s\n",
                 output_dir.AsUTF8Unsafe().c_str());
    return results;
  }
  if (!kiwi::base::CreateDirectory(output_dir)) {
    std::fprintf(stderr, "Failed to create texture output directory: %s\n",
                 output_dir.AsUTF8Unsafe().c_str());
    return results;
  }

  std::vector<kiwi::base::FilePath> pack_dirs;
  kiwi::base::FileEnumerator directories(
      textures_dir, false, kiwi::base::FileEnumerator::DIRECTORIES);
  for (kiwi::base::FilePath path = directories.Next(); !path.empty();
       path = directories.Next()) {
    pack_dirs.push_back(std::move(path));
  }
  std::sort(pack_dirs.begin(), pack_dirs.end());

  for (const kiwi::base::FilePath& pack_dir : pack_dirs) {
    kiwi::base::FilePath result = PackMesenHDTexture(pack_dir, output_dir);
    if (result.empty()) {
      results.clear();
      return results;
    }
    results.push_back(std::move(result));
  }
  return results;
}
