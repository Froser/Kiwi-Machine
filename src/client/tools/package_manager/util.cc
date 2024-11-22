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

#include "util.h"
#include "../third_party/nlohmann_json/json.hpp"
#include "base/strings/string_util.h"
#include "kiwi_nes.h"
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"
#include "third_party/zlib-1.3/contrib/minizip/zip.h"

namespace {

static const std::string g_package_manifest_template =
    u8R"({
"titles": {
  "en": "Package Test",
  "zh": "包测试",
  "ja": "テスト"
},
"icons": {
  "normal": "<?xml version=\"1.0\" encoding=\"utf-8\"?><svg fill=\"#FFFFFF\" width=\"800px\" height=\"800px\" viewBox=\"0 0 24 24\" role=\"img\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M21.809 5.524 12.806.179l-.013-.007.078-.045h-.166a1.282 1.282 0 0 0-1.196.043l-.699.403-8.604 4.954a1.285 1.285 0 0 0-.644 1.113v10.718c0 .46.245.884.644 1.113l9.304 5.357c.402.232.898.228 1.297-.009l9.002-5.345c.39-.231.629-.651.629-1.105V6.628c0-.453-.239-.873-.629-1.104zm-19.282.559L11.843.719a.642.642 0 0 1 .636.012l9.002 5.345a.638.638 0 0 1 .207.203l-4.543 2.555-4.498-2.7a.963.963 0 0 0-.968-.014L6.83 8.848 2.287 6.329a.644.644 0 0 1 .24-.246zm14.13 8.293-4.496-2.492V6.641a.32.32 0 0 1 .155.045l4.341 2.605v5.085zm-4.763-1.906 4.692 2.601-4.431 2.659-4.648-2.615a.317.317 0 0 1-.115-.112l4.502-2.533zm-.064 10.802-9.304-5.357a.643.643 0 0 1-.322-.557V7.018L6.7 9.51v5.324c0 .348.188.669.491.84l4.811 2.706.157.088v4.887a.637.637 0 0 1-.329-.083z\"/></svg>",
  "highlight": "<?xml version=\"1.0\" encoding=\"utf-8\"?><svg fill=\"#159505\" width=\"800px\" height=\"800px\" viewBox=\"0 0 24 24\" role=\"img\" xmlns=\"http://www.w3.org/2000/svg\"><path d=\"M21.809 5.524 12.806.179l-.013-.007.078-.045h-.166a1.282 1.282 0 0 0-1.196.043l-.699.403-8.604 4.954a1.285 1.285 0 0 0-.644 1.113v10.718c0 .46.245.884.644 1.113l9.304 5.357c.402.232.898.228 1.297-.009l9.002-5.345c.39-.231.629-.651.629-1.105V6.628c0-.453-.239-.873-.629-1.104zm-19.282.559L11.843.719a.642.642 0 0 1 .636.012l9.002 5.345a.638.638 0 0 1 .207.203l-4.543 2.555-4.498-2.7a.963.963 0 0 0-.968-.014L6.83 8.848 2.287 6.329a.644.644 0 0 1 .24-.246zm14.13 8.293-4.496-2.492V6.641a.32.32 0 0 1 .155.045l4.341 2.605v5.085zm-4.763-1.906 4.692 2.601-4.431 2.659-4.648-2.615a.317.317 0 0 1-.115-.112l4.502-2.533zm-.064 10.802-9.304-5.357a.643.643 0 0 1-.322-.557V7.018L6.7 9.51v5.324c0 .348.188.669.491.84l4.811 2.706.157.088v4.887a.637.637 0 0 1-.329-.083z\"/></svg>"
}
}
)";

bool ReadCurrentFileFromZip(unzFile file, std::vector<uint8_t>& data) {
  unzOpenCurrentFile(file);
  unz_file_info fi;
  unzGetCurrentFileInfo(file, &fi, nullptr, 0, nullptr, 0, nullptr, 0);
  data.resize(fi.uncompressed_size);
  bool read = unzReadCurrentFile(file, data.data(), data.size()) == data.size();
  unzCloseCurrentFile(file);
  return read;
}

bool WriteToZip(zipFile zf,
                const char* filename,
                const char* data,
                size_t size) {
  zip_fileinfo info{0};
  int err = zipOpenNewFileInZip(zf, filename, &info, nullptr, 0, nullptr, 0,
                                nullptr, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
  if (err != ZIP_OK)
    return false;

  err = zipWriteInFileInZip(zf, data, size);
  if (err != ZIP_OK)
    return false;

  zipCloseFileInZip(zf);
  return true;
}

}  // namespace

bool IsPackageExtension(const std::string& filename) {
  kiwi::base::FilePath file_path =
      kiwi::base::FilePath::FromUTF8Unsafe(filename);
  return file_path.Extension() == FILE_PATH_LITERAL(".zip");
}

bool IsJPEGExtension(const std::string& filename) {
  kiwi::base::FilePath file_path =
      kiwi::base::FilePath::FromUTF8Unsafe(filename);
  return file_path.Extension() == FILE_PATH_LITERAL(".jpg") ||
         file_path.Extension() == FILE_PATH_LITERAL(".jpeg");
}

bool IsNESExtension(const std::string& filename) {
  kiwi::base::FilePath file_path =
      kiwi::base::FilePath::FromUTF8Unsafe(filename);
  return file_path.Extension() == FILE_PATH_LITERAL(".nes");
}

ROMS ReadZipFromFile(const kiwi::base::FilePath& path) {
  static const std::vector<ROM> g_no_result;
  std::vector<ROM> result;
  unzFile file = unzOpen(path.AsUTF8Unsafe().c_str());
  if (file) {
    int err = unzLocateFile(file, "manifest.json", false);
    if (err != UNZ_OK) {
      unzClose(file);
      return g_no_result;
    }

    std::vector<uint8_t> manifest_data;
    if (!ReadCurrentFileFromZip(file, manifest_data)) {
      unzClose(file);
      return g_no_result;
    }

    manifest_data.push_back(0);  // String terminator
    manifest_data.push_back(0);  // String terminator
    nlohmann::json manifest_json = nlohmann::json::parse(manifest_data.data());
    if (manifest_json.contains("titles")) {
      const auto& titles = manifest_json.at("titles");
      for (const auto& rom_item : titles.items()) {
        ROM rom;
        rom.key = rom_item.key();
        for (const auto& title : rom_item.value().items()) {
          if (title.key() == "zh") {
            std::string zh = title.value();
            strncpy(rom.zh, zh.c_str(), ROM::MAX);
          } else if (title.key() == "zh-hint") {
            std::string zh_hint = title.value();
            strncpy(rom.zh_hint, zh_hint.c_str(), ROM::MAX);
          } else if (title.key() == "ja") {
            std::string jp = title.value();
            strncpy(rom.ja, jp.c_str(), ROM::MAX);
          } else if (title.key() == "ja-hint") {
            std::string jp_hint = title.value();
            strncpy(rom.ja_hint, jp_hint.c_str(), ROM::MAX);
          }
        }

        // Gets the cover jpg
        std::string cover_path;
        if (kiwi::base::EqualsCaseInsensitiveASCII(rom.key, "default") == 0) {
          cover_path =
              path.RemoveExtension().BaseName().AsUTF8Unsafe() + ".jpg";
        } else {
          cover_path = rom.key + ".jpg";
        }

        err = unzLocateFile(file, cover_path.c_str(), false);
        if (err != UNZ_OK) {
          // Bad cover
          unzClose(file);
          return g_no_result;
        }

        if (!ReadCurrentFileFromZip(file, rom.cover_data)) {
          unzClose(file);
          return g_no_result;
        }

        // Gets ROM data.
        std::string nes_name;
        if (kiwi::base::EqualsCaseInsensitiveASCII(rom.key, "default") == 0)
          nes_name = path.RemoveExtension().BaseName().AsUTF8Unsafe() + ".nes";
        else
          nes_name = rom.key + ".nes";

        err = unzLocateFile(file, nes_name.c_str(), false);
        if (err != UNZ_OK)
          continue;

        if (!ReadCurrentFileFromZip(file, rom.nes_data))
          continue;

        strncpy(rom.nes_file_name, nes_name.c_str(), rom.MAX);

        result.push_back(rom);
      }
    } else {
      unzClose(file);
      return g_no_result;
    }

    unzClose(file);
  }

  // Sort results.
  std::sort(result.begin(), result.end(), [](const ROM& lhs, const ROM& rhs) {
    if (kiwi::base::CompareCaseInsensitiveASCII("default", lhs.key) == 0)
      return true;

    return lhs.key < rhs.key;
  });
  return result;
}

kiwi::base::FilePath WriteZip(const kiwi::base::FilePath& save_dir,
                              const ROMS& roms) {
  kiwi::base::FilePath package_name;

  // Generate manifest.json
  nlohmann::json json;
  for (const auto& rom : roms) {
    nlohmann::json titles;
    if (strlen(rom.zh) > 0)
      titles["zh"] = rom.zh;
    if (strlen(rom.zh_hint) > 0)
      titles["zh_hint"] = rom.zh_hint;
    if (strlen(rom.ja) > 0)
      titles["ja"] = rom.zh;
    if (strlen(rom.ja_hint) > 0)
      titles["ja_hint"] = rom.ja_hint;
    json["titles"][rom.key] = titles;

    if (kiwi::base::CompareCaseInsensitiveASCII("default", rom.key) == 0) {
      package_name = kiwi::base::FilePath::FromUTF8Unsafe(
          kiwi::base::FilePath::FromUTF8Unsafe(std::string(rom.nes_file_name))
              .RemoveExtension()
              .AsUTF8Unsafe() +
          ".zip");
    }
    if (package_name.empty()) {
      // No default rom. Abort.
      return kiwi::base::FilePath();
    }
  }

  std::string manifest_contents = json.dump(2);
  // Changes \n into \r\n
  SDL_assert(manifest_contents.find('\r') == std::string::npos);
  for (size_t i = 0; i < manifest_contents.length(); ++i) {
    if (manifest_contents[i] == '\n') {
      manifest_contents.replace(i, 1, "\r\n");
      i += 1;
    }
  }

  std::string output = save_dir.Append(package_name).AsUTF8Unsafe();
  zipFile zf = zipOpen(output.c_str(), APPEND_STATUS_CREATE);
  if (!zf) {
    return kiwi::base::FilePath();
  }

  // Writes manifest
  if (!WriteToZip(zf, "manifest.json", manifest_contents.data(),
                  manifest_contents.size())) {
    zipClose(zf, nullptr);
    return kiwi::base::FilePath();
  }

  // Writes images, and roms.
  for (const auto& rom : roms) {
    std::string base_name =
        kiwi::base::FilePath::FromUTF8Unsafe(rom.nes_file_name)
            .RemoveExtension()
            .AsUTF8Unsafe();

    if (!WriteToZip(zf, (base_name + ".nes").c_str(),
                    reinterpret_cast<const char*>(rom.nes_data.data()),
                    rom.nes_data.size())) {
      zipClose(zf, nullptr);
      return kiwi::base::FilePath();
    }

    if (!WriteToZip(zf, (base_name + ".jpg").c_str(),
                    reinterpret_cast<const char*>(rom.cover_data.data()),
                    rom.cover_data.size())) {
      zipClose(zf, nullptr);
      return kiwi::base::FilePath();
    }
  }

  zipClose(zf, nullptr);
  return kiwi::base::FilePath::FromUTF8Unsafe(output);
}

kiwi::base::FilePath PackZip(const kiwi::base::FilePath& rom_zip,
                             const kiwi::base::FilePath& save_dir) {
  std::string output =
      save_dir.Append(FILE_PATH_LITERAL("test.pak")).AsUTF8Unsafe();
  zipFile zf = zipOpen(output.c_str(), APPEND_STATUS_CREATE);
  if (!zf) {
    return kiwi::base::FilePath();
  }
  if (!WriteToZip(zf, "manifest.json", g_package_manifest_template.data(),
                  g_package_manifest_template.size())) {
    zipClose(zf, nullptr);
    return kiwi::base::FilePath();
  }

  std::optional<std::vector<uint8_t>> zip_contents =
      kiwi::base::ReadFileToBytes(rom_zip);
  SDL_assert(zip_contents);
  if (!WriteToZip(zf, rom_zip.BaseName().AsUTF8Unsafe().c_str(),
                  reinterpret_cast<const char*>(zip_contents->data()),
                  zip_contents->size())) {
    zipClose(zf, nullptr);
    return kiwi::base::FilePath();
  }

  zipClose(zf, nullptr);
  return kiwi::base::FilePath::FromUTF8Unsafe(output);
}

bool IsMapperSupported(const std::vector<uint8_t>& nes_data,
                       std::string& mapper_name) {
  if (nes_data.size() <= 0x10) {
    // Not a valid nes.
    return false;
  }
  uint8_t mapper = ((nes_data[6] >> 4) & 0xf) | (nes_data[7] & 0xf0);
  mapper_name = kiwi::base::NumberToString(mapper);
  return kiwi::nes::Mapper::IsMapperSupported(mapper);
}
