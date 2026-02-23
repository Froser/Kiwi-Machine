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
#include <regex>

#include "../third_party/libjpeg-turbo-jpeg-9f/jpeglib.h"
#include "../third_party/nlohmann_json/json.hpp"
#include "base/strings/string_util.h"
#include "kiwi_nes.h"
#include "third_party/zlib-1.3.2/contrib/minizip/unzip.h"
#include "third_party/zlib-1.3.2/contrib/minizip/zip.h"
#include "workspace.h"

DECLARE_string(km_path);

#if BUILDFLAG(IS_WIN)
#include <Windows.h>
#undef min
#undef max
#endif

constexpr int kMaxLevenshteinDistance = 10;
kiwi::base::FilePath::StringPieceType kMarksFileName =
    FILE_PATH_LITERAL("marks.json");

namespace nlohmann {
void from_json(const json& j, std::map<std::string, Explorer::Mark>& obj) {
  if (j.contains("marks")) {
    const json& marks = j["marks"];
    for (const auto& mark : marks.items()) {
      for (const auto& item : mark.value().items()) {
        obj[item.key()] = item.value();
      }
    }
  }
}
}  // namespace nlohmann

namespace {
#if BUILDFLAG(IS_WIN)
std::wstring StringToWString(const std::string& str) {
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
  std::wstring to(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), to.data(),
                      size_needed);
  return to.data();
}
#endif

static const std::string g_package_manifest_template = U8(R"({
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
)");

static const std::string g_py3_pinyin_code = U8(R"(import pinyin, sys
def getpinyin(text):
    pinyin_result = pinyin.get(text, format='strip')
    pinyin_result = pinyin_result.replace('（', ' (')
    pinyin_result = pinyin_result.replace('）', ')')
    print(pinyin_result)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        getpinyin(sys.argv[1]))");

static const std::string g_py3_kana_code = U8(R"(import pykakasi, sys
def getkana(text):
    kakasi = pykakasi.kakasi()
    kakasi.setMode(fr="J", to="H")
    conv = kakasi.getConverter()
    result=conv.do(text)
    result = result.replace('（こめ）', '（べい）')
    print(result)

if __name__ == "__main__":
    if len(sys.argv) > 1:
        getkana(sys.argv[1]))");

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
      GetWorkspace().GetTestPath().Append(FILE_PATH_LITERAL("temp.py"));
  std::ofstream out_py(tmp_py_fetch_script.AsUTF8Unsafe(), std::ios::binary);
  if (out_py.is_open()) {
    out_py.write(reinterpret_cast<const char*>(code.data()), code.size());
    out_py.close();

    // Run script
    std::string output;
#if !BUILDFLAG(IS_WIN)
    std::string command =
        "python3 " + tmp_py_fetch_script.AsUTF8Unsafe() + " \"" + args + "\"";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
      return "";
    }
    char buffer[1024];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }
    pclose(pipe);
#else
    std::wstring command = L"python3 " + tmp_py_fetch_script.value() + L" \"" +
                           StringToWString(args) + L"\"";

    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    BOOL bSuccess = CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.dwFlags = STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));
    BOOL bCreateProcessSuccess =
        CreateProcessW(NULL, (LPWSTR)command.c_str(), NULL, NULL, TRUE, 0, NULL,
                       NULL, &si, &pi);
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(hWritePipe);
    char buffer[1024];
    DWORD bytes_read;
    while (ReadFile(hReadPipe, buffer, sizeof(buffer), &bytes_read, NULL)) {
      if (bytes_read == 0) {
        break;
      }

      std::string t;
      t.resize(bytes_read);
      memcpy(t.data(), buffer, bytes_read);
      output += t;
    }
    output += "\0";
    CloseHandle(hReadPipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#endif

    std::string trimmed;
    kiwi::base::TrimString(output, "\r\n", &trimmed);
    return trimmed;
  }
  return "";
}

int LevenshteinDistance(const std::string& s1, const std::string& s2) {
  int m = s1.length();
  int n = s2.length();
  std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));
  for (int i = 0; i <= m; ++i) {
    dp[i][0] = i;
  }
  for (int j = 0; j <= n; ++j) {
    dp[0][j] = j;
  }
  for (int i = 1; i <= m; ++i) {
    for (int j = 1; j <= n; ++j) {
      if (s1[i - 1] == s2[j - 1]) {
        dp[i][j] = dp[i - 1][j - 1];
      } else {
        dp[i][j] = 1 + std::min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
      }
    }
  }
  return dp[m][n];
}

std::string RemoveQuote(const std::string& str) {
  if (str.empty())
    return str;

  std::string result;
  if (str[0] == '\"') {
    result = str.substr(1, str.size() - 1);
  }

  if (result.empty())
    return result;

  if (result[result.size() - 1] == '\"') {
    result = result.substr(0, str.size() - 2);
  }

  return result;
}

struct SampleArray {
  JSAMPARRAY data = nullptr;
  int row_bytes = 0;
  int height = 0;
};

SampleArray AllocSampleArray(int row_bytes, int height) {
  JSAMPARRAY object = new JSAMPROW[height];
  for (int i = 0; i < height; ++i) {
    object[i] = new JSAMPLE[row_bytes];
  }
  return {object, row_bytes, height};
}

void FreeSampleArray(SampleArray& object) {
  for (int i = 0; i < object.height; ++i) {
    delete[] object.data[i];
  }
  delete object.data;
  object.data = nullptr;
  object.row_bytes = 0;
  object.height = 0;
}

struct SampleArrayDeleter {
  void operator()(SampleArray& object) { FreeSampleArray(object); }
};

class ScopedSampleArray {
 public:
  ScopedSampleArray(int row_bytes, int height);
  ~ScopedSampleArray();

  JSAMPLE& data(int y, int x) {
    SDL_assert(x < object_.row_bytes && y < object_.height);
    return object_.data[y][x];
  }

  JSAMPARRAY data(int y) {
    SDL_assert(y < object_.height);
    return &object_.data[y];
  }

 private:
  SampleArray object_;
};

ScopedSampleArray::ScopedSampleArray(int row_bytes, int height) {
  object_ = AllocSampleArray(row_bytes, height);
}

ScopedSampleArray ::~ScopedSampleArray() {
  FreeSampleArray(object_);
}

// Explorer internal functions
struct ExplorerFileAndZipFileComparer {
  explicit ExplorerFileAndZipFileComparer(const ROM& r) : rom(r) {}
  const ROM& rom;

  bool operator()(const Explorer::File& rhs) {
    std::vector<std::string> normalized_titles = NormalizeROMTitle(rhs.title);
    for (const auto& title : normalized_titles) {
      if (kiwi::base::CompareCaseInsensitiveASCII(rom.nes_file_name, title) ==
          0)
        return true;
    }
    return false;
  }
};

void FetchFileNames(const kiwi::base::FilePath& dir,
                    std::vector<Explorer::File>& out) {
  out.clear();
  kiwi::base::FileEnumerator d(dir, false, kiwi::base::FileEnumerator::FILES);
  for (kiwi::base::FilePath current = d.Next(); !current.empty();
       current = d.Next()) {
    if (current.Extension() == FILE_PATH_LITERAL(".nes") ||
        current.Extension() == FILE_PATH_LITERAL(".zip")) {
      out.push_back({current.BaseName().AsUTF8Unsafe(), false, dir});
    }
  }

  std::sort(out.begin(), out.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.title < rhs.title;
  });

  // Gets marks.
  kiwi::base::FilePath marks_file_path = dir.Append(kMarksFileName);
  auto marks_contents = kiwi::base::ReadFileToBytes(marks_file_path);
  if (marks_contents) {
    marks_contents->push_back(0);
    marks_contents->push_back(0);

    std::map<std::string, Explorer::Mark> marks =
        nlohmann::json::parse(marks_contents->data());

    class ExplorerFileComparator {
     public:
      ExplorerFileComparator(const std::string& s) : target_(s) {}
      bool operator()(const Explorer::File& f) const {
        return kiwi::base::CompareCaseInsensitiveASCII(f.title, target_) == 0;
      }

     private:
      std::string target_;
    };
    for (const auto& mark : marks) {
      auto search_result = std::find_if(out.begin(), out.end(),
                                        ExplorerFileComparator(mark.first));
      if (search_result != out.end()) {
        search_result->mark = mark.second;
      }
    }
  }
}

void FetchFileMapperSupported(std::vector<Explorer::File>& nes_files) {
  for (auto& nes : nes_files) {
    nes.supported = IsMapperSupported(
        nes.dir.Append(kiwi::base::FilePath::FromUTF8Unsafe(nes.title)),
        nes.mapper);
  }
}

void GenerateCompare(const kiwi::base::FilePath& cmp_dir,
                     std::vector<Explorer::File>& input_files) {
  std::vector<Explorer::File> compared_files;
  FetchFileNames(cmp_dir, compared_files);

  for (auto& item : compared_files) {
    if (kiwi::base::FilePath::FromUTF8Unsafe(item.title).Extension() ==
        FILE_PATH_LITERAL(".zip")) {
      kiwi::base::FilePath fullpath =
          item.dir.Append(kiwi::base::FilePath::FromUTF8Unsafe(item.title));
      std::vector<ROM> roms = ReadZipFromFile(fullpath);

      for (const auto& rom : roms) {
        auto input_file_iter =
            std::find_if(input_files.begin(), input_files.end(),
                         ExplorerFileAndZipFileComparer(rom));
        if (input_file_iter != input_files.end()) {
          input_file_iter->matched = true;
          input_file_iter->compared_zip_path = fullpath;
        }
      }
    }
  }
}

}  // namespace

bool IsZipExtension(const std::string& filename) {
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

void ReplaceAndAppendUnsafe(char* original_string,
                            const std::vector<const char*>& replacements,
                            const char* append_string) {
  for (const char* replacement : replacements) {
    if (kiwi::base::EndsWith(original_string, replacement)) {
      size_t pos = std::string_view(original_string).find_last_of(replacement);
      SDL_assert(pos != std::string_view::npos);
      strcpy(original_string + pos - (size_t)strlen(replacement) + 1,
             append_string);
      return;
    }
  }
  strcat(original_string, append_string);
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
        if (manifest_json.contains("boxarts")) {
          const auto& boxarts = manifest_json.at("boxarts");
          rom.has_boxarts_size_hint = boxarts.contains(rom_item.key());
        }

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

        // Gets the boxart jpg
        std::string boxart_path;
        if (kiwi::base::EqualsCaseInsensitiveASCII(rom.key, "default") == 0) {
          boxart_path =
              path.RemoveExtension().BaseName().AsUTF8Unsafe() + ".jpg";
        } else {
          boxart_path = rom.key + ".jpg";
        }

        err = unzLocateFile(file, boxart_path.c_str(), false);
        if (err != UNZ_OK) {
          // Bad boxart
          unzClose(file);
          return g_no_result;
        }

        if (!ReadCurrentFileFromZip(file, rom.boxart_data)) {
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
      titles["ja"] = rom.ja;
    if (strlen(rom.ja_hint) > 0)
      titles["ja-hint"] = rom.ja_hint;
    json["titles"][NormalizeROMTitle(rom.key)[0]] = titles;

    std::vector<std::string> normalized_nes_filenames =
        NormalizeROMTitle(rom.nes_file_name);
    std::string normalized_nes_filename = normalized_nes_filenames[0];
    if (kiwi::base::CompareCaseInsensitiveASCII("default", rom.key) == 0) {
      package_name = kiwi::base::FilePath::FromUTF8Unsafe(
          kiwi::base::FilePath::FromUTF8Unsafe(
              std::string(normalized_nes_filename))
              .RemoveExtension()
              .AsUTF8Unsafe() +
          ".zip");
    }
    if (package_name.empty()) {
      // No default rom. Abort.
      return kiwi::base::FilePath();
    }
  }

  std::string output = save_dir.Append(package_name).AsUTF8Unsafe();
  zipFile zf = zipOpen(output.c_str(), APPEND_STATUS_CREATE);
  if (!zf) {
    return kiwi::base::FilePath();
  }

  // Writes images, and roms.
  for (const auto& rom : roms) {
    std::string base_name = kiwi::base::FilePath::FromUTF8Unsafe(
                                NormalizeROMTitle(rom.nes_file_name)[0])
                                .RemoveExtension()
                                .AsUTF8Unsafe();

    if (!WriteToZip(zf, (base_name + ".nes").c_str(),
                    reinterpret_cast<const char*>(rom.nes_data.data()),
                    rom.nes_data.size())) {
      zipClose(zf, nullptr);
      return kiwi::base::FilePath();
    }

    if (!WriteToZip(zf, (base_name + ".jpg").c_str(),
                    reinterpret_cast<const char*>(rom.boxart_data.data()),
                    rom.boxart_data.size())) {
      zipClose(zf, nullptr);
      return kiwi::base::FilePath();
    }

    // Gets width and height of boxarts, and append it to manifest:
    SDL_RWops* rw =
        SDL_RWFromConstMem(rom.boxart_data.data(), rom.boxart_data.size());
    SDL_Surface* surface = IMG_Load_RW(rw, true);
    nlohmann::json boxarts;
    auto& boxart_node = json["boxarts"][NormalizeROMTitle(rom.key)[0]];
    boxart_node["width"] = surface->w;
    boxart_node["height"] = surface->h;
    SDL_FreeSurface(surface);
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

  // Writes manifest
  if (!WriteToZip(zf, "manifest.json", manifest_contents.data(),
                  manifest_contents.size())) {
    zipClose(zf, nullptr);
    return kiwi::base::FilePath();
  }

  zipClose(zf, nullptr);
  return kiwi::base::FilePath::FromUTF8Unsafe(output);
}

kiwi::base::FilePath PackZip(
    const kiwi::base::FilePath& rom_zip,
    const kiwi::base::FilePath::StringType& package_name,
    const kiwi::base::FilePath& save_dir) {
  std::vector<kiwi::base::FilePath> result =
      PackZip(std::vector{std::make_pair(rom_zip, package_name)}, save_dir);
  if (result.empty())
    return kiwi::base::FilePath();

  return *result.begin();
}

std::vector<kiwi::base::FilePath> PackZip(
    const std::vector<std::pair<kiwi::base::FilePath,
                                kiwi::base::FilePath::StringType>>& rom_zips,
    const kiwi::base::FilePath& save_dir) {
  std::vector<kiwi::base::FilePath> result;
  for (const auto& [rom_zip, package_name] : rom_zips) {
    std::string output = save_dir.Append(package_name).AsUTF8Unsafe();
    zipFile zf = zipOpen(output.c_str(), APPEND_STATUS_CREATE);
    if (!zf) {
      continue;
    }

    // Try to read existed manifest.
    kiwi::base::FilePath manifest_path =
        rom_zip.Append(FILE_PATH_LITERAL("manifest.json"));
    auto maybe_manifest_content = kiwi::base::ReadFileToBytes(manifest_path);
    bool wrote = false;
    if (maybe_manifest_content) {
      wrote = WriteToZip(
          zf, "manifest.json",
          reinterpret_cast<const char*>(maybe_manifest_content->data()),
          maybe_manifest_content->size());
    }
    if (!wrote) {
      wrote =
          WriteToZip(zf, "manifest.json", g_package_manifest_template.data(),
                     g_package_manifest_template.size());
    }
    if (!wrote) {
      zipClose(zf, nullptr);
      continue;
    }

    kiwi::base::File::Info file_info;
    kiwi::base::GetFileInfo(rom_zip, &file_info);
    if (file_info.is_directory) {
      kiwi::base::FileEnumerator d(rom_zip, false,
                                   kiwi::base::FileEnumerator::FILES,
                                   FILE_PATH_LITERAL("*.zip"));
      for (kiwi::base::FilePath current = d.Next(); !current.empty();
           current = d.Next()) {
        std::optional<std::vector<uint8_t>> zip_contents =
            kiwi::base::ReadFileToBytes(current);
        SDL_assert(zip_contents);
        if (!WriteToZip(zf, current.BaseName().AsUTF8Unsafe().c_str(),
                        reinterpret_cast<const char*>(zip_contents->data()),
                        zip_contents->size())) {
          zipClose(zf, nullptr);
          continue;
        }
      }
    } else if (!file_info.is_symbolic_link) {
      std::optional<std::vector<uint8_t>> zip_contents =
          kiwi::base::ReadFileToBytes(rom_zip);
      SDL_assert(zip_contents);
      if (!WriteToZip(zf, rom_zip.BaseName().AsUTF8Unsafe().c_str(),
                      reinterpret_cast<const char*>(zip_contents->data()),
                      zip_contents->size())) {
        zipClose(zf, nullptr);
        continue;
      }
    }
    zipClose(zf, nullptr);
    result.push_back(kiwi::base::FilePath::FromUTF8Unsafe(output));
  }

  return result;
}

std::vector<kiwi::base::FilePath> PackEntireDirectory(
    const kiwi::base::FilePath& dir,
    const kiwi::base::FilePath& save_dir) {
  std::vector<std::pair<kiwi::base::FilePath, kiwi::base::FilePath::StringType>>
      rom_zips;
  rom_zips.push_back({dir, FILE_PATH_LITERAL("main.pak")});

  kiwi::base::FileEnumerator d(dir, false,
                               kiwi::base::FileEnumerator::DIRECTORIES);
  for (kiwi::base::FilePath current = d.Next(); !current.empty();
       current = d.Next()) {
    rom_zips.push_back(
        {current, current.BaseName().value() + FILE_PATH_LITERAL(".pak")});
  }

  return PackZip(rom_zips, save_dir);
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

std::optional<std::vector<uint8_t>> ReadMapperFast(
    const kiwi::base::FilePath& nes_file) {
  std::vector<uint8_t> header;
  FILE* file(kiwi::base::OpenFile(nes_file, "rb"));
  if (!file)
    return std::nullopt;

  header.resize(0x10);
  fseek(file, 0, SEEK_SET);
  fread(header.data(), 0x10, 1, file);

  fclose(file);
  return header;
}

bool IsMapperSupported(const std::vector<uint8_t>& nes_data,
                       std::string& mapper_name) {
  if (nes_data.size() < 3 || nes_data[0] != 'N' || nes_data[1] != 'E' ||
      nes_data[2] != 'S' || nes_data[3] != 0x1a) {
    // Not a valid nes.
    return false;
  }
  uint8_t mapper = ((nes_data[6] >> 4) & 0xf) | (nes_data[7] & 0xf0);
  mapper_name = kiwi::base::NumberToString(mapper);
  return kiwi::nes::Mapper::IsMapperSupported(mapper);
}

bool IsMapperSupported(const kiwi::base::FilePath& nes_files,
                       std::string& mapper_name) {
  auto bytes = ReadMapperFast(nes_files);
  return bytes && IsMapperSupported(*bytes, mapper_name);
}

std::vector<uint8_t> TryFetchBoxArtImage(const std::string& name,
                                         kiwi::base::FilePath* suggested_url) {
  static std::set<std::pair<kiwi::base::FilePath, kiwi::base::FilePath>>
      g_boxarts;
  if (g_boxarts.empty()) {
    kiwi::base::FileEnumerator d(GetWorkspace().GetNESBoxartsPath(), false,
                                 kiwi::base::FileEnumerator::FILES);
    for (kiwi::base::FilePath current = d.Next(); !current.empty();
         current = d.Next()) {
      if (current.Extension() == FILE_PATH_LITERAL(".jpg")) {
        g_boxarts.insert({current, current.BaseName().RemoveExtension()});
      }
    }
  }

  std::multimap<int, kiwi::base::FilePath> r;
  for (const auto& boxart : g_boxarts) {
    int dis = LevenshteinDistance(kiwi::base::FilePath::FromUTF8Unsafe(name)
                                      .RemoveExtension()
                                      .AsUTF8Unsafe(),
                                  boxart.second.AsUTF8Unsafe());
    r.insert({dis, boxart.first});
  }

  std::vector<uint8_t> result;
  if (!r.empty() && r.begin()->first < kMaxLevenshteinDistance) {
    // Read from local
    auto maybe_result = kiwi::base::ReadFileToBytes(r.begin()->second);
    if (maybe_result)
      result = *maybe_result;
    return result;
  } else {
    // Re from internet
    constexpr char kSearchUrl[] = "https://wowroms.com/en/roms/list?search=";
    if (suggested_url)
      *suggested_url =
          kiwi::base::FilePath::FromUTF8Unsafe(kSearchUrl)
              .Append(
                  kiwi::base::FilePath::FromUTF8Unsafe(name).RemoveExtension());
  }

  return std::vector<uint8_t>();
}

std::string TryGetPinyin(const std::string& chinese) {
  return RunPython3Code(g_py3_pinyin_code, chinese);
}

std::string TryGetKana(const std::string& kanji) {
  return RunPython3Code(g_py3_kana_code, kanji);
}

std::string TryGetJaTitle(const std::string& en_name) {
#if BUILDFLAG(IS_WIN)
  WCHAR app_path_str[MAX_PATH];
  GetModuleFileNameW(NULL, app_path_str, MAX_PATH);
  kiwi::base::FilePath app_path(app_path_str);
  kiwi::base::FilePath db_path =
      app_path.DirName().Append(FILE_PATH_LITERAL("db.json"));
  auto db = kiwi::base::ReadFileToBytes(db_path);
#else
  auto db = kiwi::base::ReadFileToBytes(
      kiwi::base::FilePath::FromUTF8Unsafe("db.json"));
#endif
  if (!db)
    return std::string();
  db->push_back('\0');
  db->push_back('\0');

  static const nlohmann::json kGames =
      nlohmann::json::parse(*db)["database"]["game"];
  std::multimap<int, std::pair<std::string, std::string>> result;
  for (const auto& game : kGames) {
    std::string rom_name = to_string(game["$"]["name"]);
    std::string alter_name =
        game["$"].contains("altname") ? to_string(game["$"]["altname"]) : "";
    std::string region = RemoveQuote(to_string(game["$"]["region"]));
    if (kiwi::base::CompareCaseInsensitiveASCII(region, "japan") == 0) {
      alter_name = RemoveQuote(alter_name) + U8("（日）");
    } else if (kiwi::base::CompareCaseInsensitiveASCII(region, "usa") == 0) {
      alter_name = RemoveQuote(alter_name) + U8("（米）");
    }
    int dis = LevenshteinDistance(rom_name, en_name);
    result.insert({dis, {RemoveQuote(rom_name), alter_name}});
  }
  return (!result.empty() && result.begin()->first < kMaxLevenshteinDistance)
             ? (result.begin()->second.second)
             : std::string();
}

std::string RemoveROMRegion(const std::string& str) {
  std::regex pattern(R"( \((.*)\)\.nes)");
  std::string replacement = "";
  return std::regex_replace(str, pattern, replacement);
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

  ScopedSampleArray rotated_buffer(new_width * num_components, new_height);

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
    int current_scanline = cinfo.output_scanline;
    jpeg_read_scanlines(&cinfo, buffer, 1);
    for (int src_x = 0; src_x < width; ++src_x) {
      for (int c = 0; c < num_components; ++c) {
        rotated_buffer.data(
            src_x,
            (cinfo.output_height - current_scanline - 1) * num_components + c) =
            buffer[0][src_x * num_components + c];
      }
    }
  }

  while (out_cinfo.next_scanline < out_cinfo.image_height) {
    jpeg_write_scanlines(&out_cinfo,
                         rotated_buffer.data(out_cinfo.next_scanline), 1);
  }

  jpeg_finish_compress(&out_cinfo);
  jpeg_destroy_decompress(&cinfo);
  jpeg_destroy_compress(&out_cinfo);

  std::vector<uint8_t> data(out_size);
  memcpy(data.data(), outbuffer, out_size);
  free(outbuffer);
  return data;
}

bool FillRomDetailsAutomatically(ROM& rom,
                                 const kiwi::base::FilePath& basename) {
  bool found = false;
  std::string maybe_pinyin = TryGetPinyin(rom.zh);
  if (!maybe_pinyin.empty())
    strcpy(rom.zh_hint, maybe_pinyin.c_str());
  std::string rom_name_without_region =
      RemoveROMRegion(basename.AsUTF8Unsafe());
  std::string maybe_ja_name = TryGetJaTitle(rom_name_without_region);
  if (!maybe_ja_name.empty()) {
    strcpy(rom.ja, maybe_ja_name.c_str());
    found = true;
  }
  std::string maybe_kana = TryGetKana(rom.ja);
  if (!maybe_kana.empty())
    strcpy(rom.ja_hint, maybe_kana.c_str());
  return found;
}

std::vector<uint8_t> ReadImageAsJPGFromImageData(int width,
                                                 int height,
                                                 size_t bytes_per_row,
                                                 unsigned char* data) {
  std::vector<uint8_t> result;
  jpeg_compress_struct cinfo;
  jpeg_create_compress(&cinfo);
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.input_components = 3;
  cinfo.in_color_space = JCS_RGB;
  jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, 100, TRUE);

  unsigned char* outbuffer = nullptr;
  size_t out_size;
  jpeg_mem_dest(&cinfo, &outbuffer, &out_size);
  jpeg_start_compress(&cinfo, TRUE);

  ScopedSampleArray buffer(width * 3, 1);
  while (cinfo.next_scanline < cinfo.image_height) {
    for (int x = 0; x < width; x++) {
      int offset =
          (cinfo.next_scanline * bytes_per_row) + (x * (bytes_per_row / width));
      for (int i = 0; i < 3; ++i) {
        buffer.data(0, x * 3 + i) = data[offset + i];
      }
    }

    jpeg_write_scanlines(&cinfo, buffer.data(0), 1);
  }

  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  result.resize(out_size);
  memcpy(result.data(), outbuffer, result.size());
  free(outbuffer);
  return result;
}

void PackSingleZipAndRun(const kiwi::base::FilePath& zip,
                         const kiwi::base::FilePath& save_dir) {
  if (zip.empty())
    return;

  kiwi::base::FilePath package_path =
      PackZip(zip, FILE_PATH_LITERAL("test.pak"), save_dir);
  if (!package_path.empty()) {
    kiwi::base::FilePath kiwi_machine_path_from_cmdline;
    if (!FLAGS_km_path.empty()) {
      kiwi_machine_path_from_cmdline =
          kiwi::base::FilePath::FromUTF8Unsafe(FLAGS_km_path);
    }

#if BUILDFLAG(IS_MAC)
    kiwi::base::FilePath kiwi_machine(FILE_PATH_LITERAL("kiwi_machine.app"));
    if (!kiwi_machine_path_from_cmdline.empty())
      kiwi_machine = kiwi_machine_path_from_cmdline;
    RunExecutable(kiwi_machine, {"--test-pak=" + package_path.AsUTF8Unsafe(),
                                 "--enable_debug"});
#elif BUILDFLAG(IS_WIN)
    kiwi::base::FilePath kiwi_machine(FILE_PATH_LITERAL("kiwi_machine.exe"));
    if (!kiwi_machine_path_from_cmdline.empty())
      kiwi_machine = kiwi_machine_path_from_cmdline;
    RunExecutable(
        kiwi_machine,
        {L"--test-pak=\"" + package_path.value() + L"\"", L"--enable_debug"});
#else
    kiwi::base::FilePath kiwi_machine(FILE_PATH_LITERAL("kiwi_machine"));
    if (!kiwi_machine_path_from_cmdline.empty())
      kiwi_machine = kiwi_machine_path_from_cmdline;
    RunExecutable(kiwi_machine, {"--test-pak=" + package_path.AsUTF8Unsafe(),
                                 "--enable_debug"});
#endif
  }
}

void InitializeExplorerFiles(const kiwi::base::FilePath& input_dir,
                             const kiwi::base::FilePath& cmp_dir,
                             std::vector<Explorer::File>& out) {
  FetchFileNames(input_dir, out);
  FetchFileMapperSupported(out);
  GenerateCompare(cmp_dir, out);
}

void UpdateExplorerFiles(const kiwi::base::FilePath& updated_zip_file,
                         std::vector<Explorer::File>& files) {
  std::vector<ROM> roms = ReadZipFromFile(updated_zip_file);
  for (const auto& rom : roms) {
    auto input_file_iter = std::find_if(files.begin(), files.end(),
                                        ExplorerFileAndZipFileComparer(rom));
    if (input_file_iter != files.end()) {
      input_file_iter->matched = true;
      input_file_iter->compared_zip_path = updated_zip_file;
    } else {
      input_file_iter->matched = false;
      input_file_iter->compared_zip_path = kiwi::base::FilePath();
    }
  }
}

void UpdateMarks(const kiwi::base::FilePath& save_dir,
                 const std::vector<Explorer::File>& files) {
  nlohmann::json root;
  nlohmann::json& marks = root["marks"];
  for (const auto& file : files) {
    if (file.mark != Explorer::Mark::kNoMark) {
      nlohmann::json value;
      value[file.title] = file.mark;
      marks.push_back(value);
    }
  }

  std::string contents = root.dump(2);
  kiwi::base::WriteFile(save_dir.Append(kMarksFileName), contents.data(),
                        contents.size());
}

std::vector<std::string> NormalizeROMTitle(const std::string& title) {
  std::vector<std::string> result;
  constexpr char kSearchKey[] = ", The (";
  constexpr char kReplaceKey[] = ", The";
  constexpr char kSearchKey2[] = ", A (";
  constexpr char kReplaceKey2[] = ", A";
  constexpr char kSearchKey3[] = ", The -";
  constexpr char kReplaceKey3[] = ", The";
  size_t pos;
  if (pos = title.find(kSearchKey); pos != std::string::npos) {
    std::string replacement = title;
    replacement.replace(pos, sizeof(kReplaceKey) - 1, "");
    result.push_back("The " + replacement);
    result.push_back(replacement);
  } else if (pos = title.find(kSearchKey2); pos != std::string::npos) {
    std::string replacement = title;
    replacement.replace(pos, sizeof(kReplaceKey2) - 1, "");
    result.push_back("A " + replacement);
    result.push_back(replacement);
  } else if (pos = title.find(kSearchKey3); pos != std::string::npos) {
    // e.g. Jetsons, The - Cogswell's Caper (USA)
    std::string replacement = title;
    replacement.replace(pos, sizeof(kReplaceKey3) - 1, "");
    result.push_back("The " + replacement);
    result.push_back(replacement);
  }

  result.push_back(title);
  SDL_assert(!result.empty());
  return result;
}