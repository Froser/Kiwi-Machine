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

#include "utility/zip_reader.h"

#include <SDL.h>

#include "preset_roms/preset_roms.h"
#include "third_party/nlohmann_json/json.hpp"
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"
#include "ui/application.h"
#include "utility/localization.h"

namespace {
voidpf OpenFileImpl(voidpf opaque, const char* ops, int mode) {
  return (SDL_RWops*)(ops);
}

uLong ReadFileImpl(voidpf opaque, voidpf stream, void* buf, uLong size) {
  SDL_RWops* ops = reinterpret_cast<SDL_RWops*>(stream);
  return SDL_RWread(ops, buf, size, 1) * size;
}

long TellFileImpl(voidpf opaque, voidpf stream) {
  SDL_RWops* ops = reinterpret_cast<SDL_RWops*>(stream);
  return SDL_RWtell(ops);
}

int CloseFileImpl(voidpf opaque, voidpf stream) {
  SDL_RWops* ops = reinterpret_cast<SDL_RWops*>(stream);
  return SDL_RWclose(ops);
}

long SeekFileImpl(voidpf opaque, voidpf stream, uLong offset, int whence) {
  // In minizip's unzip, this function will return 0 if succeeded, and return 1
  // if failed.
  SDL_RWops* ops = reinterpret_cast<SDL_RWops*>(stream);
  if (SDL_RWseek(ops, offset, whence) != -1)
    return 0;

  return -1;
}

int ErrorFileImpl(voidpf opaque, voidpf stream) {
  return 0;
}

bool ReadCurrentFileFromZip(unzFile file, kiwi::nes::Bytes& data) {
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
                     kiwi::nes::Bytes& data) {
  int err = unzLocateFile(file, name.c_str(), false);
  if (err != UNZ_OK)
    return false;

  return ReadCurrentFileFromZip(file, data);
}

unzFile unzOpenFromMemory(kiwi::nes::Byte* data, size_t size) {
  SDL_RWops* ops = SDL_RWFromMem(const_cast<kiwi::nes::Byte*>(data), size);

  // Filling zlib_filefunc_def struct by SDL_RWops.
  zlib_filefunc_def func;
  func.zopen_file = OpenFileImpl;
  func.zread_file = ReadFileImpl;
  func.ztell_file = TellFileImpl;
  func.zseek_file = SeekFileImpl;
  func.zclose_file = CloseFileImpl;
  func.zerror_file = ErrorFileImpl;

  unzFile file = unzOpen2(reinterpret_cast<const char*>(ops), &func);
  return file;
}

#if defined(KIWI_USE_EXTERNAL_PAK)

std::vector<preset_roms::Package*> g_packages;

class PackageImpl : public preset_roms::Package {
 public:
  PackageImpl(std::vector<preset_roms::PresetROM>&& roms,
              std::map<std::string, std::string>&& titles,
              kiwi::nes::Bytes&& icon,
              kiwi::nes::Bytes&& icon_highlight);
  ~PackageImpl() override = default;

  size_t GetRomsCount() override;
  preset_roms::PresetROM& GetRomsByIndex(size_t index) override;
  kiwi::nes::Bytes GetSideMenuImage() override;
  kiwi::nes::Bytes GetSideMenuHighlightImage() override;
  std::string GetTitleForLanguage(SupportedLanguage language) override;

 private:
  std::vector<preset_roms::PresetROM> roms_;
  std::map<std::string, std::string> titles_;
  kiwi::nes::Bytes icon_;
  kiwi::nes::Bytes icon_highlight_;
};

PackageImpl::PackageImpl(std::vector<preset_roms::PresetROM>&& roms,
                         std::map<std::string, std::string>&& titles,
                         kiwi::nes::Bytes&& icon,
                         kiwi::nes::Bytes&& icon_highlight)
    : roms_(std::move(roms)),
      titles_(std::move(titles)),
      icon_(std::move(icon)),
      icon_highlight_(std::move(icon_highlight)) {}

size_t PackageImpl::GetRomsCount() {
  return roms_.size();
}

preset_roms::PresetROM& PackageImpl::GetRomsByIndex(size_t index) {
  return roms_.at(index);
}

kiwi::nes::Bytes PackageImpl::GetSideMenuImage() {
  return icon_;
}

kiwi::nes::Bytes PackageImpl::GetSideMenuHighlightImage() {
  return icon_highlight_;
}

std::string PackageImpl::GetTitleForLanguage(SupportedLanguage language) {
  return titles_[ToLanguageCode(language)];
}

#endif

}  // namespace

void InitializePresetROM(preset_roms::PresetROM& rom_data) {
  if (!rom_data.title_loaded) {
#if !defined(KIWI_USE_EXTERNAL_PAK)
    kiwi::nes::Byte* zip_data = const_cast<kiwi::nes::Byte*>(rom_data.zip_data);
    size_t zip_size = rom_data.zip_size;
#else
    kiwi::nes::Bytes zip_data_container =
        rom_data.zip_data_loader.Run(rom_data.file_pos);
    kiwi::nes::Byte* zip_data = zip_data_container.data();
    size_t zip_size = zip_data_container.size();
#endif

    unzFile file =
        unzOpenFromMemory(const_cast<kiwi::nes::Byte*>(zip_data), zip_size);
    if (file) {
      kiwi::nes::Bytes manifest;
      // Loads title or i18 names on demand.
      std::map<std::string, std::unordered_map<std::string, std::string>>
          i18n_names;
      bool success = ReadFileFromZip(file, "manifest.json", manifest);
      bool has_manifest = !manifest.empty();
      // Parsing manifest file, to extract i18n information, and so on.
      if (has_manifest) {
        // Adds a string terminator.
        manifest.push_back(0);
        nlohmann::json manifest_json = nlohmann::json::parse(manifest.data());
        if (manifest_json.contains("titles")) {
          const auto& titles = manifest_json.at("titles");
          for (const auto& rom_version : titles.items()) {
            for (const auto& title : rom_version.value().items()) {
              i18n_names[rom_version.key()].insert(
                  {title.key(), title.value()});
            }
          }
        }
      }

      auto default_i18n_names = i18n_names.find("default");
      if (default_i18n_names != i18n_names.end()) {
        // Found the default name, which is the primary ROM's name.
        rom_data.i18n_names = default_i18n_names->second;
      }
      rom_data.title_loaded = true;

      if (!success) {
        unzClose(file);
        return;
      }

      // Find alternative roms.
      int located = unzGoToFirstFile(file);
      kiwi::base::FilePath alter_rom_path;
      std::string filename;
      filename.resize(64);
      while (located == UNZ_OK) {
        unz_file_info fi;
        unzGetCurrentFileInfo(file, &fi, filename.data(), filename.size(),
                              nullptr, 0, nullptr, 0);
        alter_rom_path = kiwi::base::FilePath::FromUTF8Unsafe(filename.c_str());

        if (alter_rom_path.RemoveExtension().BaseName().AsUTF8Unsafe() ==
            kiwi::base::StringPiece(rom_data.name)) {
          located = unzGoToNextFile(file);
          continue;
        }

        auto alternative_rom_iter = std::find_if(
            rom_data.alternates.begin(), rom_data.alternates.end(),
            [&alter_rom_path](const preset_roms::PresetROM& lhs) {
              return alter_rom_path.RemoveExtension()
                         .BaseName()
                         .AsUTF8Unsafe() == kiwi::base::StringPiece(lhs.name);
            });

        std::string alter_rom_name = alter_rom_path.BaseName().AsUTF8Unsafe();
        if (alter_rom_name != "manifest.json") {
          // Finds corresponding i18n names, and store these names.
          auto alter_i18n_names = i18n_names.find(
              alter_rom_path.BaseName().RemoveExtension().AsUTF8Unsafe());
          std::unordered_map<std::string, std::string> names;
          if (alter_i18n_names != i18n_names.end()) {
            names = alter_i18n_names->second;
          }

          if (alternative_rom_iter != rom_data.alternates.end()) {
            // Use the existing alternative rom struct.
            alternative_rom_iter->i18n_names = names;
          } else {
            preset_roms::PresetROM alternative_rom;
            alternative_rom.title_loaded = true;
            alternative_rom.owned_zip_data = false;
#if !defined(KIWI_USE_EXTERNAL_PAK)
            alternative_rom.zip_data = rom_data.zip_data;
            alternative_rom.zip_size = rom_data.zip_size;
#else
            alternative_rom.file_pos = rom_data.file_pos;
            alternative_rom.zip_data_loader = rom_data.zip_data_loader;
#endif
            // Leaky name
            std::string rom_name =
                alter_rom_path.RemoveExtension().BaseName().AsUTF8Unsafe();
            alternative_rom.name = new char[rom_name.size() + 1];
            strcpy(const_cast<char*>(alternative_rom.name), rom_name.c_str());
            alternative_rom.i18n_names = names;
            rom_data.alternates.push_back(std::move(alternative_rom));
          }
        }

        located = unzGoToNextFile(file);
      }
      unzClose(file);
    } else {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Can't load rom zip data of name %s", rom_data.name);
    }
  }
}

void LoadPresetROM(preset_roms::PresetROM& rom_data, RomPart part) {
  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner =
      Application::Get()->GetIOTaskRunner();
  SDL_assert(io_task_runner->RunsTasksInCurrentSequence());
  if (!((HasAnyPart(part & RomPart::kCover) && !rom_data.cover_loaded) ||
        (HasAnyPart(part & RomPart::kContent) && !rom_data.content_loaded)))
    return;

  // Loads cover or ROM contents on demand.
  RomPart loaded_part = RomPart::kNone;

#if !defined(KIWI_USE_EXTERNAL_PAK)
  kiwi::nes::Byte* zip_data = const_cast<kiwi::nes::Byte*>(rom_data.zip_data);
  size_t zip_size = rom_data.zip_size;
#else
  kiwi::nes::Bytes zip_data_container =
      rom_data.zip_data_loader.Run(rom_data.file_pos);
  kiwi::nes::Byte* zip_data = zip_data_container.data();
  size_t zip_size = zip_data_container.size();
#endif

  unzFile file =
      unzOpenFromMemory(const_cast<kiwi::nes::Byte*>(zip_data), zip_size);
  if (file) {
    if (HasAnyPart(part & RomPart::kCover) && !rom_data.cover_loaded) {
      if (ReadFileFromZip(file, std::string(rom_data.name) + ".jpg",
                          rom_data.rom_cover)) {
        rom_data.cover_loaded = true;
        loaded_part |= RomPart::kCover;
      }
    }

    if (HasAnyPart(part & RomPart::kContent) && !rom_data.content_loaded) {
      if (ReadFileFromZip(file, std::string(rom_data.name) + ".nes",
                          rom_data.rom_data)) {
        rom_data.content_loaded = true;
        loaded_part |= RomPart::kContent;
      }
    }

    unzClose(file);
  }
}

#if defined(KIWI_USE_EXTERNAL_PAK)

// Reads all roms' data from package file.
void OpenRomDataFromPackage(std::vector<preset_roms::PresetROM>& roms,
                            std::map<std::string, std::string>& titles,
                            kiwi::nes::Bytes& icon,
                            kiwi::nes::Bytes& icon_highlight,
                            const kiwi::base::FilePath& package) {
  unzFile pak = unzOpen(package.AsUTF8Unsafe().c_str());
  SDL_assert(pak);

  int located = unzGoToFirstFile(pak);
  std::string filename;
  filename.resize(64);
  while (located == UNZ_OK) {
    unz_file_info fi;
    unzGetCurrentFileInfo(pak, &fi, filename.data(), filename.size(), nullptr,
                          0, nullptr, 0);

    if (strcmp(filename.data(), "manifest.json") == 0) {
      kiwi::nes::Bytes manifest_data;
      manifest_data.resize(fi.uncompressed_size);
      unzOpenCurrentFile(pak);
      unzReadCurrentFile(pak, manifest_data.data(), manifest_data.size());
      manifest_data.push_back(0); // String terminator
      manifest_data.push_back(0); // String terminator
      unzCloseCurrentFile(pak);

      nlohmann::json manifest_json =
          nlohmann::json::parse(manifest_data.data());
      const auto& menu_titles = manifest_json.at("titles");
      for (const auto& menu_title : menu_titles.items()) {
        titles.insert({menu_title.key(), menu_title.value()});
      }

      const auto& icon_data = manifest_json.at("icons");
      std::string normal = to_string(icon_data.at("normal"));
      std::string highlight = to_string(icon_data.at("highlight"));
      icon = kiwi::nes::Bytes(normal.begin(), normal.end());
      icon_highlight = kiwi::nes::Bytes(highlight.begin(), highlight.end());
      unzGoToNextFile(pak);
      continue;
    }

    unzOpenCurrentFile(pak);

    preset_roms::PresetROM rom;
    kiwi::base::FilePath filepath =
        kiwi::base::FilePath::FromUTF8Unsafe(filename.c_str());
    std::string name = filepath.RemoveExtension().AsUTF8Unsafe();

    unz_file_pos file_pos;
    unzGetFilePos(pak, &file_pos);
    rom.file_pos = file_pos;
    rom.zip_data_loader = kiwi::base::BindRepeating(
        [](const kiwi::base::FilePath& package, unz_file_pos file_pos) {
          kiwi::nes::Bytes data;
          unzFile f = unzOpen(package.AsUTF8Unsafe().c_str());
          if (f) {
            int found = unzGoToFilePos(f, &file_pos);
            if (found == UNZ_OK) {
              std::string filename;
              filename.resize(64);
              unz_file_info fi;
              unzGetCurrentFileInfo(f, &fi, filename.data(), filename.size(),
                                    nullptr, 0, nullptr, 0);
              data.resize(fi.uncompressed_size);
              unzOpenCurrentFile(f);
              int read = unzReadCurrentFile(f, data.data(), data.size());
              unzCloseCurrentFile(f);
              if (read <= 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Read current file error: %d", read);
              }

            } else {
              SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                          "Can't goto file pos: %lu, %lu", file_pos.num_of_file,
                          file_pos.pos_in_zip_directory);
            }
            unzClose(f);
          } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Open file error: %s",
                        package.AsUTF8Unsafe().c_str());
          }
          return data;
        },
        package);

    rom.name = new char[name.size() + 1];
    strncpy(const_cast<char*>(rom.name), name.data(), name.size() + 1);
    unzCloseCurrentFile(pak);
    roms.push_back(std::move(rom));
    located = unzGoToNextFile(pak);
  }

  unzClose(pak);
}

void CloseRomDataFromPackage(preset_roms::PresetROM& rom) {
  delete[] rom.name;
}

void OpenPackageFromFile(const kiwi::base::FilePath& package_path) {
  std::vector<preset_roms::PresetROM> roms;
  std::map<std::string, std::string> titles;
  kiwi::nes::Bytes icon, icon_highlight;
  OpenRomDataFromPackage(roms, titles, icon, icon_highlight, package_path);
  preset_roms::Package* package =
      new PackageImpl(std::move(roms), std::move(titles), std::move(icon),
                      std::move(icon_highlight));
  g_packages.push_back(package);
}

void ClosePackages() {
  for (preset_roms::Package* package : g_packages) {
    for (size_t i = 0; i < package->GetRomsCount(); ++i) {
      CloseRomDataFromPackage(package->GetRomsByIndex(i));
    }
    delete package;
  }
  g_packages.clear();
}

namespace preset_roms {
// Implements preset_roms.h
std::vector<preset_roms::Package*> GetPresetRomsPackages() {
  return g_packages;
}
}  // namespace preset_roms

#endif
