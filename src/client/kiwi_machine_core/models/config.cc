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

#include "models/config.h"

#include <SDL.h>
#include "build/build_config.h"

#include <exception>

#include "base/files/file.h"
#include "base/functional/bind.h"
#include "build/kiwi_defines.h"
#include "ui/application.h"

#if BUILDFLAG(IS_WASM)
#include "utility/emscripten/bridge_api.h"
#endif

namespace {
kiwi::base::FilePath GetSettingsFile(const kiwi::base::FilePath& profile_path) {
  return profile_path.Append(FILE_PATH_LITERAL("Settings"));
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

#if BUILDFLAG(IS_WASM)
  SyncFilesystem();
#endif

  return true;
}

void RemoveRedundantGameOverrides(
    ApplicationConfig::ABSwapSystemSettings* settings) {
  for (auto override = settings->game_overrides.begin();
       override != settings->game_overrides.end();) {
    if (override->second == settings->global)
      override = settings->game_overrides.erase(override);
    else
      ++override;
  }
}

}  // namespace

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ApplicationConfig::ABSwapSystemSettings,
    global,
    game_overrides);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ApplicationConfig::ABSwapSettings,
    nes,
    gba);

#if BUILDFLAG(IS_MOBILE)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApplicationConfig::Data,
                                                window_scale,
                                                is_fullscreen,
                                                volume,
                                                last_index,
                                                last_system,
                                                is_stretch_mode,
                                                language,
                                                swap_ab_settings);
#else
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ApplicationConfig::Data,
                                                window_scale,
                                                is_fullscreen,
                                                volume,
                                                last_index,
                                                last_system,
                                                language,
                                                swap_ab_settings);
#endif

bool ApplicationConfig::ABSwapSystemSettings::HasGameOverride(
    const std::string& game_id) const {
  return !game_id.empty() && game_overrides.contains(game_id);
}

const ApplicationConfig::ABSwapValues&
ApplicationConfig::ABSwapSystemSettings::GetEffective(
    const std::string& game_id) const {
  auto override = game_overrides.find(game_id);
  return override == game_overrides.end() ? global : override->second;
}

bool ApplicationConfig::ABSwapSystemSettings::SetGameValue(
    const std::string& game_id,
    int player,
    bool swapped) {
  if (game_id.empty() || player < 0 ||
      player >= static_cast<int>(global.size()))
    return false;

  auto override = game_overrides.find(game_id);
  if (override == game_overrides.end()) {
    if (global[player] == swapped)
      return false;
    override = game_overrides.emplace(game_id, global).first;
  } else if (override->second[player] == swapped &&
             override->second != global) {
    return false;
  }

  override->second[player] = swapped;
  if (override->second == global)
    game_overrides.erase(override);
  return true;
}

ApplicationConfig::ApplicationConfig(const kiwi::base::FilePath& profile_path)
    : profile_path_(profile_path) {}

ApplicationConfig::~ApplicationConfig() = default;

void ApplicationConfig::LoadConfigAndWait() {
  LoadFromUTF8Json(LoadConfigBlocked(profile_path_));
}

void ApplicationConfig::SaveConfig() {
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&SaveConfigOnIOThread, profile_path_, DataToJson()),
      kiwi::base::BindOnce(&ApplicationConfig::OnConfigSaved,
                           kiwi::base::Unretained(this)));
}

std::string ApplicationConfig::DataToJson() {
  nlohmann::json j = data_;
  return j.dump();
}

void ApplicationConfig::LoadFromUTF8Json(const std::string& utf8_json) {
  if (!utf8_json.empty()) {
    try {
      nlohmann::json json = nlohmann::json::parse(utf8_json);
      data_ = json;
      if (!json.contains("swap_ab_settings")) {
        auto legacy_swap_ab = json.find("swap_ab");
        if (legacy_swap_ab != json.end()) {
          const ABSwapValues values = legacy_swap_ab->get<ABSwapValues>();
          data_.swap_ab_settings.nes.global = values;
          data_.swap_ab_settings.gba.global = values;
        }
      }
      RemoveRedundantGameOverrides(&data_.swap_ab_settings.nes);
      RemoveRedundantGameOverrides(&data_.swap_ab_settings.gba);
    } catch (const std::exception& e) {
      SDL_LogWarn(
          SDL_LOG_CATEGORY_APPLICATION,
          "Parsing config file failed, exception: %s. Try to create a new one.",
          e.what());
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // On android, window scale factor always sets to 4.0, because it is the best
  // scaling.
  data_.window_scale = 4.f;
#elif BUILDFLAG(IS_IOS)
  // On iOS, SDL_WINDOW_ALLOW_HIGHDPI is set, the bounds will be in points
  // instead of pixels.
  data_.window_scale = 1.f;
#endif
}

void ApplicationConfig::OnConfigSaved(bool success) {
  if (!success) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Can't save config for profile %s",
                profile_path_.AsUTF8Unsafe().c_str());
  }
}
