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

#include "models/nes_config.h"

#include "ui/application.h"

namespace {
kiwi::base::FilePath GetSettingsFile(const kiwi::base::FilePath& profile_path) {
  return profile_path.Append("Settings");
}

std::string LoadConfigBlocked(const kiwi::base::FilePath& profile_path) {
  kiwi::base::File file(
      GetSettingsFile(profile_path),
      kiwi::base::File::FLAG_OPEN | kiwi::base::File::FLAG_READ);

  if (!file.IsValid())
    return std::string();

  std::string content;
  content.resize(file.GetLength());
  if (!content.empty()) {
    file.ReadAtCurrentPos(content.data(), content.size());
  }
  return content;
}

bool SaveConfigOnIOThread(const kiwi::base::FilePath& profile_path,
                          const std::string& content) {
  kiwi::base::File file(
      GetSettingsFile(profile_path),
      kiwi::base::File::FLAG_WRITE | kiwi::base::File::FLAG_CREATE);

  if (!file.IsValid())
    return false;

  file.WriteAtCurrentPos(content.data(), content.size());
  return true;
}

}  // namespace

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(NESConfig::Data,
                                   window_scale,
                                   is_fullscreen,
                                   volume);

NESConfig::NESConfig(const kiwi::base::FilePath& profile_path)
    : profile_path_(profile_path) {}

NESConfig::~NESConfig() = default;

void NESConfig::LoadConfigAndWait() {
  LoadFromUTF8Json(LoadConfigBlocked(profile_path_));
}

void NESConfig::SaveConfig() {
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&SaveConfigOnIOThread, profile_path_, DataToJson()),
      kiwi::base::BindOnce(&NESConfig::OnConfigSaved,
                           kiwi::base::Unretained(this)));
}

std::string NESConfig::DataToJson() {
  nlohmann::json j = data_;
  return j.dump();
}

void NESConfig::LoadFromUTF8Json(const std::string& utf8_json) {
  if (!utf8_json.empty()) {
    data_ = nlohmann::json::parse(utf8_json);
  }
}

void NESConfig::OnConfigSaved(bool success) {
  if (!success) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Can't save config for profile %s",
                profile_path_.AsUTF8Unsafe().c_str());
  }
}