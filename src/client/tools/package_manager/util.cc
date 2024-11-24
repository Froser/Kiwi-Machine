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

extern "C" {
#include "../third_party/jpeg-6b/jerror.h"
#include "../third_party/jpeg-6b/jpeglib.h"
}

#include "../third_party/nlohmann_json/json.hpp"
#include "base/strings/string_util.h"
#include "kiwi_nes.h"
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"
#include "third_party/zlib-1.3/contrib/minizip/zip.h"

namespace {
void init_source(j_decompress_ptr cinfo) {}
boolean fill_input_buffer(j_decompress_ptr cinfo) {
  ERREXIT(cinfo, JERR_INPUT_EMPTY);
  return TRUE;
}
void skip_input_data(j_decompress_ptr cinfo, long num_bytes) {
  struct jpeg_source_mgr* src = (struct jpeg_source_mgr*)cinfo->src;

  if (num_bytes > 0) {
    src->next_input_byte += (size_t)num_bytes;
    src->bytes_in_buffer -= (size_t)num_bytes;
  }
}
void term_source(j_decompress_ptr cinfo) {}

void jpeg_mem_src(j_decompress_ptr cinfo, void* buffer, long nbytes) {
  struct jpeg_source_mgr* src;

  if (cinfo->src == NULL) { /* first time for this JPEG object? */
    cinfo->src = (struct jpeg_source_mgr*)(*cinfo->mem->alloc_small)(
        (j_common_ptr)cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_source_mgr));
  }

  src = (struct jpeg_source_mgr*)cinfo->src;
  src->init_source = init_source;
  src->fill_input_buffer = fill_input_buffer;
  src->skip_input_data = skip_input_data;
  src->resync_to_restart = jpeg_resync_to_restart; /* use default method */
  src->term_source = term_source;
  src->bytes_in_buffer = nbytes;
  src->next_input_byte = (JOCTET*)buffer;
}

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


def search_site_content(search, download_path):
    params = quote(search)
    url = "https://cse.google.com/cse/element/v1?rsz=filtered_cse&num=10&hl=en&source=gcsc&cselibv=8fa85d58e016b414&cx=002230802627130757280%3Afp34oki149w&safe=off&cse_tok=AB-tC_5R4I7tN1fbraZkXo7ZzdeE%3A1732451055997&sort=&exp=cc%2Capo&callback=google.search.cse.api13082&q=" + params

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
            if images[0][-4:] == ".jpg":
                dst_path = Path(download_path) / "tmp_cover.jpg"
                req = Request(images[0], headers=headers, method="GET")
                response = urlopen(req, context=context)
                with open(dst_path, 'wb') as file:
                    block_size = 1024
                    while True:
                        data = response.read(block_size)
                        if not data:
                            break
                        file.write(data)
                    print(f"{dst_path}")
    except Exception as e:
        print(f"Exception{e}")


if __name__ == "__main__":
    if len(sys.argv) > 2:
        search = sys.argv[1]
        download_path = sys.argv[2]
        search_site_content(search, download_path))T";

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
  kiwi::base::FilePath tmp_py_fetch_script =
      GetDefaultSavePath().Append(FILE_PATH_LITERAL("temp_fetch.py"));
  std::ofstream out_py(tmp_py_fetch_script.AsUTF8Unsafe(), std::ios::binary);
  if (out_py.is_open()) {
    out_py.write(reinterpret_cast<const char*>(g_py3_fetch_code.data()),
                 g_py3_fetch_code.size());
    out_py.close();

    // Run script
    std::string command =
        "python3 " + tmp_py_fetch_script.AsUTF8Unsafe() + " \"" + name + "\"";
    command += " \"" + GetDefaultSavePath().AsUTF8Unsafe() + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
      return kiwi::base::FilePath();
    }
    char buffer[1024];
    std::string output;
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }
    pclose(pipe);
    std::string trimmed_path;
    kiwi::base::TrimString(output, "\r\n", &trimmed_path);
    return kiwi::base::FilePath::FromUTF8Unsafe(trimmed_path);
  }

  return kiwi::base::FilePath();
}

std::vector<uint8_t> RotateJPEG(std::vector<uint8_t> input_data) {
  jpeg_decompress_struct cinfo{0};
  jpeg_compress_struct out_cinfo{0};

  jpeg_mem_src(&cinfo, input_data.data(), input_data.size());
  jpeg_read_header(&cinfo, true);
  jpeg_start_decompress(&cinfo);

  int width = cinfo.output_width;
  int height = cinfo.output_height;
  int num_components = cinfo.output_components;

  int new_width = height, new_height = width;
  JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)(
      (j_common_ptr)&cinfo, JPOOL_IMAGE, new_width * num_components, 1);
  JSAMPARRAY rotated_buffer = (*cinfo.mem->alloc_sarray)(
      (j_common_ptr)&cinfo, JPOOL_IMAGE, new_width * num_components, 1);

  kiwi::base::FilePath output =
      GetDefaultSavePath().Append(FILE_PATH_LITERAL("tmp_rotated_cover.jpg"));
  std::string output_filename = output.AsUTF8Unsafe();
  jpeg_create_compress(&out_cinfo);
  FILE* outfile = fopen(output_filename.c_str(), "wb");
  if (outfile == nullptr) {
    jpeg_destroy_decompress(&cinfo);
    jpeg_destroy_compress(&out_cinfo);
    return std::vector<uint8_t>();
  }
  jpeg_stdio_dest(&out_cinfo, outfile);
  out_cinfo.image_width = new_width;
  out_cinfo.image_height = new_height;
  out_cinfo.input_components = num_components;
  if (num_components == 3) {
    out_cinfo.in_color_space = JCS_RGB;
  } else if (num_components == 4) {
    return std::vector<uint8_t>();
  } else {
    fclose(outfile);
    jpeg_destroy_decompress(&cinfo);
    jpeg_destroy_compress(&out_cinfo);
    return std::vector<uint8_t>();
  }
  jpeg_set_defaults(&out_cinfo);
  jpeg_set_quality(&out_cinfo, 85, TRUE);
  jpeg_start_compress(&out_cinfo, TRUE);

  double cos_angle = cos(90 * (M_PI / 180.0));
  double sin_angle = sin(90 * (M_PI / 180.0));
  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, buffer, 1);
    for (int y = 0; y < new_height; ++y) {
      for (int x = 0; x < new_width; ++x) {
        int src_x = (x - new_width / 2) * cos_angle +
                    (y - new_height / 2) * sin_angle + width / 2;
        int src_y = -(x - new_width / 2) * sin_angle +
                    (y - new_height / 2) * cos_angle + height / 2;
        if (src_x >= 0 && src_x < width && src_y >= 0 && src_y < height) {
          for (int c = 0; c < num_components; ++c) {
            rotated_buffer[y][x * num_components + c] =
                buffer[src_y][src_x * num_components + c];
          }
        }
      }
    }
    jpeg_write_scanlines(&out_cinfo, rotated_buffer, 1);
  }

  jpeg_finish_compress(&out_cinfo);
  fclose(outfile);
  jpeg_destroy_decompress(&cinfo);
  jpeg_destroy_compress(&out_cinfo);

  auto data = kiwi::base::ReadFileToBytes(output);
  return *data;
}
