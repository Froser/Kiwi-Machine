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

#ifndef UTIL_H_
#define UTIL_H_

#include <SDL.h>
#include <gflags/gflags.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

#define U8(x) (reinterpret_cast<const char*>(u8##x))

class ROMWindow;
struct ROM {
  friend class ROMWindow;

  static constexpr size_t MAX = 128;

  // Title
  std::string key;
  char zh[MAX]{0};
  char zh_hint[MAX]{0};
  char ja[MAX]{0};
  char ja_hint[MAX]{0};

  // Cover (boxart)
  std::vector<uint8_t> boxart_data;

  std::vector<uint8_t> nes_data;
  char nes_file_name[MAX]{0};

 private:
  SDL_Texture* boxart_texture_ = nullptr;
};

using ROMS = std::vector<ROM>;

bool IsZipExtension(const std::string& filename);
bool IsJPEGExtension(const std::string& filename);
bool IsNESExtension(const std::string& filename);

void ReplaceAndAppendUnsafe(char* original_string,
                            const std::vector<const char*>& replacements,
                            const char* append_string);

[[nodiscard]] ROMS ReadZipFromFile(const kiwi::base::FilePath& path);
kiwi::base::FilePath WriteZip(const kiwi::base::FilePath& save_dir,
                              const ROMS& roms);
std::vector<kiwi::base::FilePath> PackZip(
    const std::vector<std::pair<kiwi::base::FilePath,
                                kiwi::base::FilePath::StringType>>& rom_zips,
    const kiwi::base::FilePath& save_dir);
kiwi::base::FilePath PackZip(
    const kiwi::base::FilePath& rom_zip,
    const kiwi::base::FilePath::StringType& package_name,
    const kiwi::base::FilePath& save_dir);
// Packs the entire directory.
// If the root contains manifest.json, uses it. Otherwise, use a template
// manifest.json for testing. If the root contains subdirectories, it will
// generate its package as well.
std::vector<kiwi::base::FilePath> PackEntireDirectory(
    const kiwi::base::FilePath& dir,
    const kiwi::base::FilePath& save_dir);
kiwi::base::FilePath WriteROM(const char* filename,
                              const std::vector<uint8_t>& data,
                              const kiwi::base::FilePath& dir);
bool IsMapperSupported(const std::vector<uint8_t>& nes_data,
                       std::string& mapper_name);
bool IsMapperSupported(const kiwi::base::FilePath& nes_files,
                       std::string& mapper_name);

#if BUILDFLAG(IS_WIN)
kiwi::base::FilePath GetFontsPath();
#endif

void ShellOpen(const kiwi::base::FilePath& file);
void ShellOpenDirectory(const kiwi::base::FilePath& file);
#if BUILDFLAG(IS_MAC)
void RunExecutable(const kiwi::base::FilePath& bundle,
                   const std::vector<std::string>& args);
#else
void RunExecutable(const kiwi::base::FilePath& executable,
                   const std::vector<std::wstring>& args);
#endif

std::vector<uint8_t> TryFetchBoxArtImage(const std::string& name,
                                         kiwi::base::FilePath* suggested_url);
std::string TryGetPinyin(const std::string& chinese);
std::string TryGetKana(const std::string& kanji);
std::string TryGetJaTitle(const std::string& en_name);
std::string RemoveROMRegion(const std::string& str);
std::vector<uint8_t> RotateJPEG(std::vector<uint8_t> cover_data);
bool FillRomDetailsAutomatically(ROM& rom,
                                 const kiwi::base::FilePath& filename);
std::vector<uint8_t> ReadImageAsJPGFromClipboard();
std::vector<uint8_t> ReadImageAsJPGFromImageData(int width,
                                                 int height,
                                                 size_t bytes_per_row,
                                                 unsigned char* data);
void PackSingleZipAndRun(const kiwi::base::FilePath& zip,
                         const kiwi::base::FilePath& save_dir);

// Explorer
struct Explorer {
  enum class Mark {
    kNoMark,
    kUninterested,
    kImprefect,
  };

  struct File {
    std::string title;
    bool selected;
    kiwi::base::FilePath dir;
    bool matched;
    bool supported;
    std::string mapper;
    kiwi::base::FilePath compared_zip_path;
    Mark mark = Mark::kNoMark;
  };
  std::vector<File> explorer_files;
};

void InitializeExplorerFiles(const kiwi::base::FilePath& input_dir,
                             const kiwi::base::FilePath& cmp_dir,
                             std::vector<Explorer::File>& out);
void UpdateExplorerFiles(const kiwi::base::FilePath& updated_zip_file,
                         std::vector<Explorer::File>& files);
void UpdateMarks(const kiwi::base::FilePath& save_dir,
                 const std::vector<Explorer::File>& files);

// Some roms may have special naming rules:
// XXX, The (USA).nes, XXX, A (USA).nes will be adjusted into two possible
// names: The XXX (USA).nes, A XXX (USA).nes
// Returns the suggested normalized rom title. Always use the first one if
// possible.
std::vector<std::string> NormalizeROMTitle(const std::string& title);

#endif  // UTIL_H_