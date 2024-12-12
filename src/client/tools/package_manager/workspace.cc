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

#include "workspace.h"

#include <gflags/gflags.h>

#include "base/files/file_util.h"

DEFINE_string(workspace, "", "Default workspace.");

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Workspace::Manifest,
                                   nes_roms_dir,
                                   zipped_nes_dir,
                                   nes_boxarts_dir);

// A simple workspace json config:
// {
//   "nes_roms_dir": "roms/nes",
//   "zipped_nes_dir": "zipped/nes",
//   "nes_boxarts_dir": "boxarts/nes"
// }

Workspace::Workspace() {
  strcpy(workspace_dir, FLAGS_workspace.c_str());
  workspace_path_ = kiwi::base::FilePath::FromUTF8Unsafe(workspace_dir);
  bool read = false;
  if (strlen(workspace_dir) > 0) {
    kiwi::base::FilePath manifest_path =
        workspace_path_.Append(FILE_PATH_LITERAL("manifest.json"));
    read = ReadFromManifest(manifest_path);
  }

  if (!read) {
    // Use default manifest:
    manifest_.nes_roms_dir = "roms/nes";
    manifest_.zipped_nes_dir = "zipped/nes";
    manifest_.nes_boxarts_dir = "boxarts/nes";
  }
}

bool Workspace::ReadFromManifest(const kiwi::base::FilePath& manifest_file) {
  std::optional<std::vector<uint8_t>> manifest_contents =
      kiwi::base::ReadFileToBytes(manifest_file);
  if (manifest_contents) {
    manifest_contents->push_back(0);
    manifest_contents->push_back(0);

    nlohmann::json object = nlohmann::json::parse(manifest_contents->data());
    from_json(object, manifest_);
    return true;
  }
  return false;
}

kiwi::base::FilePath Workspace::GetOutPath() {
  kiwi::base::FilePath out_path =
      kiwi::base::FilePath::FromUTF8Unsafe(workspace_dir)
          .Append(FILE_PATH_LITERAL("out"));
  if (!kiwi::base::DirectoryExists(out_path)) {
    bool created = kiwi::base::CreateDirectory(out_path);
    if (!created)
      return kiwi::base::FilePath();
  }
  return out_path;
}

kiwi::base::FilePath Workspace::GetZippedPath() {
  kiwi::base::FilePath zip_output_path = workspace_path_.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(manifest_.zipped_nes_dir));
  if (!kiwi::base::DirectoryExists(zip_output_path)) {
    bool created = kiwi::base::CreateDirectory(zip_output_path);
    if (!created)
      return kiwi::base::FilePath();
  }
  return zip_output_path;
}

kiwi::base::FilePath Workspace::GetTestPath() {
  kiwi::base::FilePath test_path =
      GetOutPath().Append(FILE_PATH_LITERAL("test"));
  if (!kiwi::base::DirectoryExists(test_path)) {
    bool created = kiwi::base::CreateDirectory(test_path);
    if (!created)
      return kiwi::base::FilePath();
  }
  return test_path;
}

kiwi::base::FilePath Workspace::GetPackageOutputPath() {
  kiwi::base::FilePath output_path =
      GetOutPath().Append(FILE_PATH_LITERAL("output"));
  if (!kiwi::base::DirectoryExists(output_path)) {
    bool created = kiwi::base::CreateDirectory(output_path);
    if (!created)
      return kiwi::base::FilePath();
  }
  return output_path;
}

kiwi::base::FilePath Workspace::GetNESRomsPath() {
  kiwi::base::FilePath nes_roms_path = workspace_path_.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(manifest_.nes_roms_dir));
  if (!kiwi::base::DirectoryExists(nes_roms_path)) {
    bool created = kiwi::base::CreateDirectory(nes_roms_path);
    if (!created)
      return kiwi::base::FilePath();
  }
  return nes_roms_path;
}

kiwi::base::FilePath Workspace::GetNESBoxartsPath() {
  kiwi::base::FilePath nes_boxarts_path = workspace_path_.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(manifest_.nes_boxarts_dir));
  if (!kiwi::base::DirectoryExists(nes_boxarts_path)) {
    bool created = kiwi::base::CreateDirectory(nes_boxarts_path);
    if (!created)
      return kiwi::base::FilePath();
  }
  return nes_boxarts_path;
}

Workspace& GetWorkspace() {
  static Workspace settings;
  return settings;
}
