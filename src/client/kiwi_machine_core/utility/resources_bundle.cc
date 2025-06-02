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

#include "utility/resources_bundle.h"

#include <SDL.h>
#include <unordered_map>

#include "base/strings/sys_string_conversions.h"
#include "third_party/nlohmann_json/json.hpp"
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"
#include "utility/localization.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {
constexpr char kManifestFileName[] = "manifest.json";
unzFile g_package_handle;
using StringMap = std::vector<std::unordered_map<std::string, std::string>>;
StringMap g_resource_paths;
const std::string kInvalidString = std::string();

bool ReadCurrentFileFromZip(unzFile file, std::vector<std::byte>& data) {
  unzOpenCurrentFile(file);
  unz_file_info fi;
  unzGetCurrentFileInfo(file, &fi, nullptr, 0, nullptr, 0, nullptr, 0);
  data.resize(fi.uncompressed_size);
  bool read = unzReadCurrentFile(file, data.data(), data.size()) == data.size();
  unzCloseCurrentFile(file);
  return read;
}

bool ReadFileFromZip(unzFile file,
                     const std::string& name,
                     std::vector<std::byte>& data) {
  int err = unzLocateFile(file, name.c_str(), false);
  if (err != UNZ_OK)
    return false;

  return ReadCurrentFileFromZip(file, data);
}

bool LoadResourceFromJSON(const std::string& json_contents) {
  nlohmann::json j = nlohmann::json::parse(json_contents.c_str());
  if (j) {
    g_resource_paths.clear();
    if (!j.is_array())
      return false;

    const auto& a = j.items();
    for (const auto& i : j) {
      if (i.is_object()) {
        std::unordered_map<std::string, std::string> strs;
        if (i.contains("en")) {
          std::string en = i["en"].get<std::string>();
          strs.insert(std::make_pair("en", std::move(en)));
        }

        if (i.contains("zh")) {
          std::string zh = i["zh"].get<std::string>();
          strs.insert(std::make_pair("zh", std::move(zh)));
        }

        if (i.contains("ja")) {
          std::string ja = i["ja"].get<std::string>();
          strs.insert(std::make_pair("ja", std::move(ja)));
        }

        g_resource_paths.push_back(std::move(strs));
      }
    }
    return true;
  }

  return false;
}

std::string GetResourcePath(int id) {
  if (id < 0 || id >= g_resource_paths.size())
    return kInvalidString;

  SupportedLanguage language = GetCurrentSupportedLanguage();
  std::string res_path;
  const auto& resource_path = g_resource_paths[id];
  const char* app_language = ToLanguageCode(language);
  auto lang_iter = resource_path.find(app_language);
  if (lang_iter == resource_path.end()) {
    lang_iter = resource_path.find("en");
  }

  if (lang_iter == resource_path.end())
    return kInvalidString;

  return lang_iter->second;
}

std::string ToSysPath(const kiwi::base::FilePath& path) {
  std::string path_in_sys_encoding;
#if BUILDFLAG(IS_WIN)
  path_in_sys_encoding =
      kiwi::base::SysWideToMultiByte(path.value(), 0 /* CP_ACP */);
#else
  path_in_sys_encoding = path.AsUTF8Unsafe();
#endif
  return path_in_sys_encoding;
}

}  // namespace

bool LoadResourceFromPackage(const kiwi::base::FilePath& package) {
  SDL_assert(!g_package_handle);
  // 读取实际资源
  std::string sys_path = ToSysPath(package);
  g_package_handle = unzOpen(sys_path.c_str());

  std::vector<std::byte> manifest_file_content;
  if (!g_package_handle)
    return false;

  if (!ReadFileFromZip(g_package_handle, kManifestFileName,
                       manifest_file_content))
    return false;

  // 增加终止字符
  manifest_file_content.push_back(static_cast<std::byte>(0));

  // 读取相关的描述文件
  if (!LoadResourceFromJSON(
          reinterpret_cast<const char*>(manifest_file_content.data())))
    return false;

  return g_package_handle;
}

std::optional<std::vector<std::byte>> GetResource(int id) {
  if (!g_package_handle || g_resource_paths.empty()) {
    return std::nullopt;
  }

  std::string real_path = GetResourcePath(id);
  if (real_path.empty())
    return std::nullopt;

  std::vector<std::byte> data;
  bool success = ReadFileFromZip(g_package_handle, real_path, data);
  if (success)
    return data;

  return std::nullopt;
}

void ClosePackage() {
  if (g_package_handle)
    unzClose(g_package_handle);
}
