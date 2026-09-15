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

#include "models/battery_save.h"

#include <optional>
#include <span>

#if defined(_WIN32)
#include <windows.h>
#else
#include <cstdio>
#endif

#include "base/files/file_util.h"
#include "build/kiwi_defines.h"
#include "nes/rom_hash.h"

#if KIWI_WASM
#include "utility/emscripten/bridge_api.h"
#endif

namespace battery_save {
namespace {

bool ReplaceSaveFile(const kiwi::base::FilePath& temporary_path,
                     const kiwi::base::FilePath& save_path) {
#if defined(_WIN32)
  return ::MoveFileExW(temporary_path.value().c_str(),
                       save_path.value().c_str(),
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
#else
  return std::rename(temporary_path.value().c_str(),
                     save_path.value().c_str()) == 0;
#endif
}

}  // namespace

std::optional<kiwi::base::FilePath> GetPath(
    const kiwi::base::FilePath& profile_path,
    const std::string& rom_sha1) {
  std::optional<std::string> normalized = kiwi::nes::NormalizeSha1Hex(rom_sha1);
  if (!normalized) {
    return std::nullopt;
  }

  return profile_path.Append(FILE_PATH_LITERAL("Saves"))
      .Append(kiwi::base::FilePath::FromUTF8Unsafe(*normalized + ".sav"));
}

ReadResult ReadFromPath(const kiwi::base::FilePath& save_path) {
  constexpr int64_t kMaximumSaveSize = 1024 * 1024;
  if (!kiwi::base::PathExists(save_path)) {
    return ReadResult{true, false, {}};
  }

  kiwi::base::File file(
      save_path, kiwi::base::File::FLAG_OPEN | kiwi::base::File::FLAG_READ);
  if (!file.IsValid()) {
    return ReadResult{false, true, {}};
  }

  const int64_t file_size = file.GetLength();
  if (file_size <= 0 || file_size > kMaximumSaveSize) {
    return ReadResult{false, true, {}};
  }
  kiwi::nes::Bytes data(static_cast<size_t>(file_size));
  if (file.Read(0, reinterpret_cast<char*>(data.data()),
                static_cast<int>(data.size())) != file_size) {
    return ReadResult{false, true, {}};
  }
  return ReadResult{true, true, std::move(data)};
}

ReadResult Read(const kiwi::base::FilePath& profile_path,
                const std::string& rom_sha1) {
  std::optional<kiwi::base::FilePath> save_path =
      GetPath(profile_path, rom_sha1);
  return save_path ? ReadFromPath(*save_path) : ReadResult{};
}

bool Write(const kiwi::base::FilePath& profile_path,
           const std::string& rom_sha1,
           const kiwi::nes::Bytes& data) {
  std::optional<kiwi::base::FilePath> save_path =
      GetPath(profile_path, rom_sha1);
  return save_path && WriteToPath(*save_path, data);
}

bool WriteToPath(const kiwi::base::FilePath& save_path,
                 const kiwi::nes::Bytes& data) {
  if (save_path.empty() || data.empty() ||
      !kiwi::base::CreateDirectory(save_path.DirName())) {
    return false;
  }

  const kiwi::base::FilePath temporary_path =
      kiwi::base::FilePath::FromUTF8Unsafe(save_path.AsUTF8Unsafe() + ".tmp");
  if (!kiwi::base::WriteFile(temporary_path, std::span<const uint8_t>(data))) {
    kiwi::base::DeletePathRecursively(temporary_path);
    return false;
  }
  if (!ReplaceSaveFile(temporary_path, save_path)) {
    kiwi::base::DeletePathRecursively(temporary_path);
    return false;
  }

#if KIWI_WASM
  SyncFilesystem();
#endif
  return true;
}

std::optional<kiwi::base::FilePath> BackupInvalid(
    const kiwi::base::FilePath& save_path) {
  if (save_path.empty() || !kiwi::base::PathExists(save_path)) {
    return std::nullopt;
  }

  constexpr size_t kMaximumBackupAttempts = 1000;
  const std::string backup_base = save_path.AsUTF8Unsafe() + ".invalid";
  for (size_t suffix = 0; suffix < kMaximumBackupAttempts; ++suffix) {
    const std::string backup_name =
        backup_base +
        (suffix == 0 ? ".bak" : "." + std::to_string(suffix) + ".bak");
    kiwi::base::FilePath backup_path =
        kiwi::base::FilePath::FromUTF8Unsafe(backup_name);
    if (kiwi::base::PathExists(backup_path)) {
      continue;
    }
    if (kiwi::base::CopyFile(save_path, backup_path)) {
#if KIWI_WASM
      SyncFilesystem();
#endif
      return backup_path;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

}  // namespace battery_save
