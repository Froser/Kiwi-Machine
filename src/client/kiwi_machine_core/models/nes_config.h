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

#ifndef NES_CONFIG_H_
#define NES_CONFIG_H_

#include <kiwi_nes.h>
#include <string>

#include "build/kiwi_defines.h"
#include "third_party/nlohmann_json/json.hpp"

class NESConfig : public kiwi::base::RefCounted<NESConfig> {
  friend class kiwi::base::RefCounted<NESConfig>;

 public:
  struct Data {
    float window_scale = 3.f;
    bool is_fullscreen = false;
    float volume = 1.f;
    int last_index = 0;
#if KIWI_MOBILE
    bool is_stretch_mode = true;
#endif
  };

 private:
  ~NESConfig();

 public:
  explicit NESConfig(const kiwi::base::FilePath& profile_path);

  Data& data() { return data_; }

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

#endif  // NES_CONFIG_H_