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

#include "utility/zip_reader.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "nes/rom_hash.h"
#include "preset_roms/preset_roms.h"
#include "third_party/googletest-release-1.12.1/googletest/include/gtest/gtest.h"
#include "third_party/nlohmann_json/json.hpp"
#include "third_party/zlib-1.3.2/contrib/minizip/zip.h"

namespace {

struct ZipEntry {
  std::string name;
  std::string contents;
};

bool WriteZip(const kiwi::base::FilePath& path,
              const std::vector<ZipEntry>& entries) {
  zipFile archive = zipOpen(path.AsUTF8Unsafe().c_str(), APPEND_STATUS_CREATE);
  if (!archive) {
    return false;
  }

  bool success = true;
  for (const ZipEntry& entry : entries) {
    zip_fileinfo info = {};
    if (zipOpenNewFileInZip(archive, entry.name.c_str(), &info, nullptr, 0,
                            nullptr, 0, nullptr, Z_DEFLATED,
                            Z_DEFAULT_COMPRESSION) != ZIP_OK ||
        zipWriteInFileInZip(archive, entry.contents.data(),
                            static_cast<unsigned int>(entry.contents.size())) !=
            ZIP_OK ||
        zipCloseFileInZip(archive) != ZIP_OK) {
      success = false;
      break;
    }
  }
  return zipClose(archive, nullptr) == ZIP_OK && success;
}

std::string ReadFile(const kiwi::base::FilePath& path) {
  std::optional<std::vector<uint8_t>> bytes = kiwi::base::ReadFileToBytes(path);
  return bytes ? std::string(bytes->begin(), bytes->end()) : std::string();
}

std::string Sha1(std::string_view value) {
  const auto* bytes = reinterpret_cast<const uint8_t*>(value.data());
  return kiwi::nes::CalculateSha1Hex(
      std::span<const uint8_t>(bytes, value.size()));
}

class PackageIndexTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ClosePackages();
    const std::string directory_name =
        "kiwi_package_index_" +
        std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count());
    root_path_ = kiwi::base::FilePath::FromUTF8Unsafe(
        (std::filesystem::temp_directory_path() / directory_name).string());
    profile_path_ = root_path_.Append(FILE_PATH_LITERAL("Default"));
    package_path_ = root_path_.Append(FILE_PATH_LITERAL("main.pak"));
    nested_path_ = root_path_.Append(FILE_PATH_LITERAL("Game.zip"));
    ASSERT_TRUE(kiwi::base::CreateDirectory(profile_path_));
  }

  void TearDown() override {
    ClosePackages();
    EXPECT_TRUE(kiwi::base::DeletePathRecursively(root_path_));
  }

  void CreatePackage(std::string rom_contents = "rom-data") {
    const std::string inner_manifest =
        nlohmann::json(
            {{"titles",
              {{"default", {{"en", "Game"}}},
               {"Game (Japan)", {{"en", "Game Japan"}}}}},
             {"boxarts",
              {{"default", {{"width", 160}, {"height", 224}}},
               {"Game (Japan)", {{"width", 160}, {"height", 224}}}}}})
            .dump();
    ASSERT_TRUE(
        WriteZip(nested_path_, {{"manifest.json", inner_manifest},
                                {"Game.nes", rom_contents},
                                {"Game.jpg", "cover"},
                                {"Game (Japan).nes", "alternate-rom"},
                                {"Game (Japan).jpg", "alternate-cover"}}));

    const std::string outer_manifest =
        nlohmann::json(
            {{"titles", {{"en", "Test Package"}}},
             {"icons", {{"normal", "normal"}, {"highlight", "highlight"}}},
             {"rom_sha1s",
              {{"Game",
                {{"Game", Sha1(rom_contents)},
                 {"Game (Japan)", Sha1("alternate-rom")}}}}}})
            .dump();
    ASSERT_TRUE(
        WriteZip(package_path_, {{"manifest.json", outer_manifest},
                                 {"Game.zip", ReadFile(nested_path_)}}));
  }

  preset_roms::Package* OpenPackage() {
    OpenPackageFromFile(package_path_);
    const std::vector<preset_roms::Package*> packages =
        preset_roms::GetPresetOrTestRomsPackages();
    EXPECT_EQ(packages.size(), 1u);
    return packages.empty() ? nullptr : packages.front();
  }

  kiwi::base::FilePath GetOnlyCacheFile() {
    kiwi::base::FileEnumerator files(
        profile_path_.Append(FILE_PATH_LITERAL("PackageIndex")), false,
        kiwi::base::FileEnumerator::FILES, FILE_PATH_LITERAL("*.json"));
    kiwi::base::FilePath result = files.Next();
    EXPECT_FALSE(result.empty());
    EXPECT_TRUE(files.Next().empty());
    return result;
  }

  kiwi::base::FilePath root_path_;
  kiwi::base::FilePath profile_path_;
  kiwi::base::FilePath package_path_;
  kiwi::base::FilePath nested_path_;
};

TEST_F(PackageIndexTest, BuildsAndRestoresPackageMetadata) {
  CreatePackage();
  preset_roms::Package* package = OpenPackage();
  ASSERT_NE(package, nullptr);

  std::vector<std::pair<size_t, size_t>> progress;
  ASSERT_TRUE(BuildPackageIndex(
      package, profile_path_,
      kiwi::base::BindRepeating(
          [](std::vector<std::pair<size_t, size_t>>* values, size_t completed,
             size_t total) { values->emplace_back(completed, total); },
          &progress)));
  EXPECT_TRUE(IsPackageIndexReady(package));
  ASSERT_EQ(progress.size(), 2u);
  EXPECT_EQ(progress.front(), std::make_pair(size_t{0}, size_t{1}));
  EXPECT_EQ(progress.back(), std::make_pair(size_t{1}, size_t{1}));
  ASSERT_FALSE(GetOnlyCacheFile().empty());

  ClosePackages();
  package = OpenPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_FALSE(IsPackageIndexReady(package));
  ASSERT_TRUE(RestorePackageIndexFromCache(package, profile_path_));
  EXPECT_TRUE(IsPackageIndexReady(package));

  preset_roms::PresetROM& rom = package->GetRomsByIndex(0);
  EXPECT_EQ(rom.i18n_names.at("en"), "Game");
  EXPECT_EQ(rom.boxart_width, 160);
  EXPECT_EQ(rom.boxart_height, 224);
  EXPECT_EQ(rom.sha1, Sha1("rom-data"));
  ASSERT_EQ(rom.alternates.size(), 1u);
  EXPECT_STREQ(rom.alternates[0].name, "Game (Japan)");
  EXPECT_EQ(rom.alternates[0].i18n_names.at("en"), "Game Japan");
}

TEST_F(PackageIndexTest, RejectsCacheWhenPackageContentChanges) {
  CreatePackage();
  preset_roms::Package* package = OpenPackage();
  ASSERT_NE(package, nullptr);
  ASSERT_TRUE(BuildPackageIndex(package, profile_path_, {}));
  ClosePackages();

  CreatePackage("changed-rom-data");
  package = OpenPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_FALSE(RestorePackageIndexFromCache(package, profile_path_));
  EXPECT_FALSE(IsPackageIndexReady(package));
}

TEST_F(PackageIndexTest, RejectsCachedFilePositionThatDoesNotMatchArchive) {
  CreatePackage();
  preset_roms::Package* package = OpenPackage();
  ASSERT_NE(package, nullptr);
  ASSERT_TRUE(BuildPackageIndex(package, profile_path_, {}));
  const kiwi::base::FilePath cache_path = GetOnlyCacheFile();

  nlohmann::json cache = nlohmann::json::parse(ReadFile(cache_path));
  cache["roms"][0]["file_position"]["num_of_file"] =
      cache["roms"][0]["file_position"]["num_of_file"].get<uint64_t>() + 1;
  cache["payload_sha1"] = Sha1(cache["roms"].dump());
  const std::string modified_cache = cache.dump();
  ASSERT_EQ(kiwi::base::WriteFile(cache_path, modified_cache.data(),
                                  static_cast<int>(modified_cache.size())),
            static_cast<int>(modified_cache.size()));

  ClosePackages();
  package = OpenPackage();
  ASSERT_NE(package, nullptr);
  EXPECT_FALSE(RestorePackageIndexFromCache(package, profile_path_));
  EXPECT_FALSE(IsPackageIndexReady(package));
}

}  // namespace
