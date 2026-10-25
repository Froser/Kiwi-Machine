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

#ifndef MODELS_CONFIG_H_
#define MODELS_CONFIG_H_

#include <array>
#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "build/kiwi_defines.h"
#include "third_party/nlohmann_json/json.hpp"

class ApplicationConfig : public kiwi::base::RefCounted<ApplicationConfig> {
  friend class kiwi::base::RefCounted<ApplicationConfig>;

 public:
  using ABSwapValues = std::array<bool, 2>;

  struct ABSwapSystemSettings {
    ABSwapValues global = {};
    std::map<std::string, ABSwapValues> game_overrides;

    bool HasGameOverride(const std::string& game_id) const;
    const ABSwapValues& GetEffective(const std::string& game_id) const;
    bool SetGameValue(const std::string& game_id, int player, bool swapped);
  };

  struct ABSwapSettings {
    ABSwapSystemSettings nes;
    ABSwapSystemSettings gba;
  };

  struct Data {
    float window_scale = 3.f;
    bool is_fullscreen = false;
    float volume = 1.f;
    int last_index = 0;
    int last_system = 0;
    int language = -1;  // -1 means automatic (follows system's locale)
    ABSwapSettings swap_ab_settings;
#if BUILDFLAG(IS_MOBILE)
    bool is_stretch_mode = true;
#endif
  };

 private:
  ~ApplicationConfig();

 public:
  explicit ApplicationConfig(const kiwi::base::FilePath& profile_path);

  Data& data() { return data_; }
  const Data& data() const { return data_; }

  // LoadConfigAndWait will block current thread, read config from disk device.
  void LoadConfigAndWait();
  void SaveConfig();

 private:
  std::string DataToJson();
  void LoadFromUTF8Json(const std::string& utf8_json);
  void OnConfigSaved(bool success);

 private:
  kiwi::base::FilePath profile_path_;
  Data data_;
};

#endif  // MODELS_CONFIG_H_
