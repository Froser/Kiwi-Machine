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

#ifndef MODELS_BATTERY_SAVE_H_
#define MODELS_BATTERY_SAVE_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "nes/types.h"

namespace battery_save {

struct ReadResult {
  bool success = false;
  bool exists = false;
  kiwi::nes::Bytes data;
};

std::optional<kiwi::base::FilePath> GetPath(
    const kiwi::base::FilePath& profile_path,
    const std::string& rom_sha1);
ReadResult Read(const kiwi::base::FilePath& profile_path,
                const std::string& rom_sha1);
ReadResult ReadFromPath(const kiwi::base::FilePath& save_path);
bool Write(const kiwi::base::FilePath& profile_path,
           const std::string& rom_sha1,
           const kiwi::nes::Bytes& data);
bool WriteToPath(const kiwi::base::FilePath& save_path,
                 const kiwi::nes::Bytes& data);
std::optional<kiwi::base::FilePath> BackupInvalid(
    const kiwi::base::FilePath& save_path);

}  // namespace battery_save

#endif  // MODELS_BATTERY_SAVE_H_
