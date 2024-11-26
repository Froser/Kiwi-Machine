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

#include <SDL_image.h>
#include <stdio.h>
#include <csetjmp>

#include "../third_party/libjpeg-turbo-jpeg-9f/jpeglib.h"
#include "../third_party/nlohmann_json/json.hpp"
#include "base/strings/string_util.h"
#include "kiwi_nes.h"
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"
#include "third_party/zlib-1.3/contrib/minizip/zip.h"

Settings& GetSettings() {
  static Settings settings;
  return settings;
}

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

static const std::string g_py3_fetch_code =
    u8R"T(from urllib.parse import quote
from urllib.request import Request, urlopen
import ssl
import re
import sys
import random
from pathlib import Path

user_agents = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/540.30 (KHTML, HTML5) Chrome/91.0.4472.124 Safari/537.36",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, HTML5) Chrome/89.0.4389.82 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, HTML5) Chrome/91.0.4472.124 Safari/537.36",
    "Mozilla/5.0 (Linux; Android 10; SM-G980F) AppleWebKit/537.36 (KHTML, HTML5) Chrome/89.0.4389.82 Mobile Safari/537.36"
]


def search_site_content(urlbase, search, download_path):
    params = quote(search)
    url = urlbase + params

    headers = {
        "User-Agent": random.choice(user_agents)
    }

    req = Request(url, headers=headers, method="GET")

    try:
        context = ssl._create_unverified_context()
        response = urlopen(req, context=context)
        html = response.read().decode('utf-8')
        link_pattern = re.compile(r'"src": "(.*?)"')
        images = link_pattern.findall(html)
        if len(images) > 0:
            for i in range(len(images)):
                if images[i][-4:] == ".jpg":
                    dst_path = Path(download_path) / "tmp_cover.jpg"
                    req = Request(images[i], headers=headers, method="GET")
                    response = urlopen(req, context=context)
                    with open(dst_path, 'wb') as file:
                        block_size = 1024
                        while True:
                            data = response.read(block_size)
                            if not data:
                                break
                            file.write(data)
                        print(f"{dst_path}")
                    break
    except Exception as e:
        print(f"Exception{e}")


if __name__ == "__main__":
    if len(sys.argv) > 3:
        search = sys.argv[1]
        download_path = sys.argv[2]
        urlbase = sys.argv[3]
        search_site_content(urlbase, search, download_path))T";

static const std::string g_py3_pinyin_code = R"(import pinyin, sys
def getpinyin(text):
    pinyin_result = pinyin.get(text, format='strip')
    pinyin_result = pinyin_result.replace('（', ' (')
    pinyin_result = pinyin_result.replace('）', ')')
    print(pinyin_result)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        getpinyin(sys.argv[1]))";

static const std::string g_py3_kana_code = R"(import pykakasi, sys
def getkana(text):
    kakasi = pykakasi.kakasi()
    kakasi.setMode(fr="J", to="H")
    conv = kakasi.getConverter()
    result=conv.do(text)
    result = result.replace('（こめ）', '（べい）')
    print(result)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        getkana(sys.argv[1]))";

struct WorkerData {
  kiwi::base::OnceClosure runnable;
};

int SDLCALL WorkerThread(void* data) {
  WorkerData* worker_data = static_cast<WorkerData*>(data);
  std::move(worker_data->runnable).Run();
  delete worker_data;
  return 0;
}

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

std::string RunPython3Code(const std::string& code, const std::string& args) {
  kiwi::base::FilePath tmp_py_fetch_script =
      GetDefaultSavePath().Append(FILE_PATH_LITERAL("temp.py"));
  std::ofstream out_py(tmp_py_fetch_script.AsUTF8Unsafe(), std::ios::binary);
  if (out_py.is_open()) {
    out_py.write(reinterpret_cast<const char*>(code.data()), code.size());
    out_py.close();

    // Run script
    std::string command =
        "python3 " + tmp_py_fetch_script.AsUTF8Unsafe() + " \"" + args + "\"";
    command += " \"" + GetDefaultSavePath().AsUTF8Unsafe() + "\" \"" +
               GetSettings().cover_query_url + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
      return "";
    }
    char buffer[1024];
    std::string output;
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }
    pclose(pipe);

    std::string trimmed;
    kiwi::base::TrimString(output, "\r\n", &trimmed);
    return trimmed;
  }
  return "";
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
            strcpy(rom.zh, zh.c_str());
          } else if (title.key() == "zh-hint") {
            std::string zh_hint = title.value();
            strcpy(rom.zh_hint, zh_hint.c_str());
          } else if (title.key() == "ja") {
            std::string jp = title.value();
            strcpy(rom.ja, jp.c_str());
          } else if (title.key() == "ja-hint") {
            std::string jp_hint = title.value();
            strcpy(rom.ja_hint, jp_hint.c_str());
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

        strcpy(rom.nes_file_name, nes_name.c_str());

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
      titles["zh-hint"] = rom.zh_hint;
    if (strlen(rom.ja) > 0)
      titles["ja"] = rom.zh;
    if (strlen(rom.ja_hint) > 0)
      titles["ja-hint"] = rom.ja_hint;
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

kiwi::base::FilePath WriteROM(const char* filename,
                              const std::vector<uint8_t>& data,
                              const kiwi::base::FilePath& dir) {
  kiwi::base::FilePath output =
      dir.Append(kiwi::base::FilePath::FromUTF8Unsafe(filename));

  std::ofstream out_rom(output.AsUTF8Unsafe(), std::ios::binary);
  if (out_rom.is_open()) {
    out_rom.write(reinterpret_cast<const char*>(data.data()), data.size());
    out_rom.close();
    return output;
  } else {
    return kiwi::base::FilePath();
  }
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

kiwi::base::FilePath TryFetchCoverImage(const std::string& name) {
  std::string output = RunPython3Code(g_py3_fetch_code, name);
  if (!output.empty()) {
    return kiwi::base::FilePath::FromUTF8Unsafe(output);
  }

  return kiwi::base::FilePath();
}

std::string TryGetPinyin(const std::string& chinese) {
  return RunPython3Code(g_py3_pinyin_code, chinese);
}

std::string TryGetKana(const std::string& kanji) {
  return RunPython3Code(g_py3_kana_code, kanji);
}

std::vector<uint8_t> RotateJPEG(std::vector<uint8_t> input_data) {
  jpeg_decompress_struct cinfo;
  jpeg_compress_struct out_cinfo;
  struct jpeg_error_mgr jerr;

  jpeg_create_decompress(&cinfo);
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_mem_src(&cinfo, input_data.data(), input_data.size());
  jpeg_read_header(&cinfo, TRUE);
  jpeg_start_decompress(&cinfo);

  int width = cinfo.output_width;
  int height = cinfo.output_height;
  int num_components = cinfo.output_components;

  // int new_width = height, new_height = width;
  int new_width = height, new_height = width;
  JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)(
      (j_common_ptr)&cinfo, JPOOL_IMAGE, width * num_components, 1);

  jpeg_create_compress(&out_cinfo);
  JSAMPARRAY rotated_buffer =
      (*cinfo.mem->alloc_sarray)((j_common_ptr)&out_cinfo, JPOOL_IMAGE,
                                 new_width * num_components, new_height);

  unsigned char* outbuffer = nullptr;
  size_t out_size;
  out_cinfo.image_width = new_width;
  out_cinfo.image_height = new_height;
  out_cinfo.input_components = num_components;
  if (num_components == 3) {
    out_cinfo.in_color_space = JCS_RGB;
  } else if (num_components == 4) {
    return std::vector<uint8_t>();
  } else {
    free(outbuffer);
    jpeg_destroy_decompress(&cinfo);
    jpeg_destroy_compress(&out_cinfo);
    return std::vector<uint8_t>();
  }
  out_cinfo.err = jpeg_std_error(&jerr);
  jpeg_set_defaults(&out_cinfo);
  jpeg_set_quality(&out_cinfo, 100, TRUE);
  jpeg_mem_dest(&out_cinfo, &outbuffer, &out_size);
  jpeg_start_compress(&out_cinfo, TRUE);

  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, buffer, 1);
    for (int src_x = 0; src_x < width; ++src_x) {
      for (int c = 0; c < num_components; ++c) {
        rotated_buffer[width - src_x - 1]
                      [cinfo.output_scanline * num_components + c] =
                          buffer[0][src_x * num_components + c];
      }
    }
  }

  while (out_cinfo.next_scanline < out_cinfo.image_height) {
    jpeg_write_scanlines(&out_cinfo, &rotated_buffer[out_cinfo.next_scanline],
                         1);
  }

  jpeg_finish_compress(&out_cinfo);
  jpeg_destroy_decompress(&cinfo);
  jpeg_destroy_compress(&out_cinfo);

  std::vector<uint8_t> data(out_size);
  memcpy(data.data(), outbuffer, out_size);
  free(outbuffer);
  return data;
}

void RunThread(kiwi::base::OnceClosure runnable) {
  WorkerData* worker_data = new WorkerData();
  worker_data->runnable = std::move(runnable);
  SDL_Thread* g_leaky_thread =
      SDL_CreateThread(WorkerThread, "Worker", worker_data);
  SDL_DetachThread(g_leaky_thread);
}
