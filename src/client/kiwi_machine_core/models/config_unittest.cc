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

#include "models/config.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"

namespace {

class ApplicationConfigTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::string directory_name =
        "kiwi_config_" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    profile_path_ = kiwi::base::FilePath::FromUTF8Unsafe(
        (std::filesystem::temp_directory_path() / directory_name).string());
    ASSERT_TRUE(kiwi::base::CreateDirectory(profile_path_));
  }

  void TearDown() override {
    EXPECT_TRUE(kiwi::base::DeletePathRecursively(profile_path_));
  }

  void WriteSettings(const std::string& json) {
    std::ofstream settings(
        profile_path_.Append(FILE_PATH_LITERAL("Settings")).AsUTF8Unsafe());
    ASSERT_TRUE(settings);
    settings << json;
    ASSERT_TRUE(settings);
  }

  kiwi::base::FilePath profile_path_;
};

TEST(ABSwapSystemSettingsTest, GameValuesLazilyOverrideAndRestoreGlobalValues) {
  ApplicationConfig::ABSwapSystemSettings settings;
  settings.global = {true, false};

  EXPECT_EQ(settings.GetEffective("game"), settings.global);
  EXPECT_FALSE(settings.HasGameOverride("game"));
  EXPECT_FALSE(settings.SetGameValue("", 0, false));
  EXPECT_FALSE(settings.SetGameValue("game", -1, false));
  EXPECT_TRUE(settings.game_overrides.empty());

  EXPECT_TRUE(settings.SetGameValue("game", 1, true));
  ASSERT_TRUE(settings.HasGameOverride("game"));
  EXPECT_EQ(settings.GetEffective("game"),
            (ApplicationConfig::ABSwapValues{true, true}));
  EXPECT_EQ(settings.GetEffective("another-game"),
            (ApplicationConfig::ABSwapValues{true, false}));

  EXPECT_TRUE(settings.SetGameValue("game", 1, false));
  EXPECT_FALSE(settings.HasGameOverride("game"));
  EXPECT_EQ(settings.GetEffective("game"),
            (ApplicationConfig::ABSwapValues{true, false}));
}

TEST_F(ApplicationConfigTest, MigratesLegacyLastIndexToNES) {
  WriteSettings(R"({"last_index":17,"last_system":0})");
  scoped_refptr<ApplicationConfig> config =
      kiwi::base::MakeRefCounted<ApplicationConfig>(profile_path_);

  config->LoadConfigAndWait();

  EXPECT_EQ(config->data().game_library_settings.nes.last_index, 17);
  EXPECT_TRUE(
      config->data().game_library_settings.nes.selected_package.empty());
  EXPECT_EQ(config->data().game_library_settings.gba.last_index, 0);
}

TEST_F(ApplicationConfigTest, MigratesLegacyLastIndexToGBA) {
  WriteSettings(R"({"last_index":23,"last_system":1})");
  scoped_refptr<ApplicationConfig> config =
      kiwi::base::MakeRefCounted<ApplicationConfig>(profile_path_);

  config->LoadConfigAndWait();

  EXPECT_EQ(config->data().game_library_settings.nes.last_index, 0);
  EXPECT_EQ(config->data().game_library_settings.gba.last_index, 23);
  EXPECT_TRUE(
      config->data().game_library_settings.gba.selected_package.empty());
}

TEST_F(ApplicationConfigTest,
       NewGameLibrarySettingsTakePrecedenceOverLegacyIndex) {
  WriteSettings(R"({
    "last_index":99,
    "last_system":1,
    "game_library_settings":{
      "nes":{"last_index":7,"selected_package":"nes_specials.pak"},
      "gba":{"last_index":11,"selected_package":"gba_main.pak"}
    }
  })");
  scoped_refptr<ApplicationConfig> config =
      kiwi::base::MakeRefCounted<ApplicationConfig>(profile_path_);

  config->LoadConfigAndWait();

  EXPECT_EQ(config->data().game_library_settings.nes.last_index, 7);
  EXPECT_EQ(config->data().game_library_settings.nes.selected_package,
            "nes_specials.pak");
  EXPECT_EQ(config->data().game_library_settings.gba.last_index, 11);
  EXPECT_EQ(config->data().game_library_settings.gba.selected_package,
            "gba_main.pak");
}

TEST_F(ApplicationConfigTest, MigratesLegacySwapValuesToBothSystems) {
  WriteSettings(R"({"swap_ab":[true,false]})");
  scoped_refptr<ApplicationConfig> config =
      kiwi::base::MakeRefCounted<ApplicationConfig>(profile_path_);

  config->LoadConfigAndWait();

  EXPECT_EQ(config->data().swap_ab_settings.nes.global,
            (ApplicationConfig::ABSwapValues{true, false}));
  EXPECT_EQ(config->data().swap_ab_settings.gba.global,
            (ApplicationConfig::ABSwapValues{true, false}));
}

TEST_F(ApplicationConfigTest, NewSwapSettingsTakePrecedenceOverLegacyValues) {
  WriteSettings(R"({
    "swap_ab":[true,true],
    "swap_ab_settings":{
      "nes":{
        "global":[false,true],
        "game_overrides":{
          "NES_GAME":[true,true],
          "REDUNDANT":[false,true]
        }
      },
      "gba":{"global":[true,false],"game_overrides":{}}
    }
  })");
  scoped_refptr<ApplicationConfig> config =
      kiwi::base::MakeRefCounted<ApplicationConfig>(profile_path_);

  config->LoadConfigAndWait();

  EXPECT_EQ(config->data().swap_ab_settings.nes.global,
            (ApplicationConfig::ABSwapValues{false, true}));
  EXPECT_EQ(config->data().swap_ab_settings.nes.GetEffective("NES_GAME"),
            (ApplicationConfig::ABSwapValues{true, true}));
  EXPECT_FALSE(
      config->data().swap_ab_settings.nes.HasGameOverride("REDUNDANT"));
  EXPECT_EQ(config->data().swap_ab_settings.gba.global,
            (ApplicationConfig::ABSwapValues{true, false}));
}

}  // namespace
