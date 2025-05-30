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
#include <SDL_image.h>
#include <utility>

#include "preset_roms/preset_roms.h"
#include "third_party/nlohmann_json/json.hpp"
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"
#include "ui/application.h"
#include "utility/localization.h"

#if KIWI_ANDROID
#include "third_party/SDL2/src/core/android/SDL_android.h"
#endif

namespace {
constexpr size_t kFileNameMaxLength = 256;

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

struct Unz : kiwi::base::RefCounted<Unz> {
  unzFile unz = nullptr;
  kiwi::nes::Bytes data;

  operator bool() { return !!unz; }
  operator unzFile() { return unz; }

  Unz(unzFile u) : unz(u) {}
  ~Unz() { unzClose(unz); }

  Unz(const Unz&) = delete;
  Unz& operator=(const Unz&) = delete;
  Unz(Unz&& rhs) { *this = std::move(rhs); }
  Unz& operator=(Unz&& rhs) {
    unz = rhs.unz;
    rhs.unz = nullptr;
    std::swap(unz, rhs.unz);
    return *this;
  }
};

scoped_refptr<Unz> OpenUnz(const kiwi::base::FilePath& file) {
#if KIWI_ANDROID
  // Android uses AssetManager, so we put the contents into memory first.
  SDL_RWops ops;
  Android_JNI_FileOpen(&ops, file.AsUTF8Unsafe().c_str(), "o");
  size_t size = Android_JNI_FileSize(&ops);
  Android_JNI_FileSeek(&ops, 0, SEEK_SET);
  kiwi::nes::Bytes data;
  data.resize(size);
  Android_JNI_FileRead(&ops, data.data(), size, size);
  Android_JNI_FileClose(&ops);

  scoped_refptr<Unz> unz = kiwi::base::MakeRefCounted<Unz>(nullptr);
  unz->data = std::move(data);
  unz->unz = unzOpenFromMemory(unz->data.data(), unz->data.size());
  return unz;
#else
  return kiwi::base::MakeRefCounted<Unz>(unzOpen(file.AsUTF8Unsafe().c_str()));
#endif
}

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

preset_roms::Region GuessROMRegion(std::string_view filename) {
  if (filename.find("(USA)") != std::string_view::npos ||
      filename.find("(US)") != std::string_view::npos ||
      filename.find("(U)") != std::string_view::npos) {
    return preset_roms::Region::kUSA;
  } else if (filename.find("(Japan)") != std::string_view::npos ||
             filename.find("(J)") != std::string_view::npos) {
    return preset_roms::Region::kJapan;
  } else if (filename.find("(CN)") != std::string_view::npos) {
    return preset_roms::Region::kCN;
  }
  return preset_roms::Region::kUnknown;
}

kiwi::nes::Bytes LoadZipDataFromFilePos(scoped_refptr<Unz> f,
                                        unz_file_pos file_pos) {
  kiwi::nes::Bytes data;
  int found = unzGoToFilePos(*f, &file_pos);
  if (found == UNZ_OK) {
    std::string filename;
    filename.resize(kFileNameMaxLength);
    unz_file_info fi;
    unzGetCurrentFileInfo(*f, &fi, filename.data(), filename.size(), nullptr, 0,
                          nullptr, 0);
    data.resize(fi.uncompressed_size);
    unzOpenCurrentFile(*f);
    int read = unzReadCurrentFile(*f, data.data(), data.size());
    unzCloseCurrentFile(*f);
    if (read <= 0) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Read current file error: %d",
                  read);
    }
  } else {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Can't goto file pos: %lu, %lu",
                file_pos.num_of_file, file_pos.pos_in_zip_directory);
  }
  return data;
}

}  // namespace

void InitializePresetROM(preset_roms::PresetROM& rom_data) {
  if (!rom_data.title_loaded) {
    kiwi::nes::Bytes zip_data_container =
        rom_data.zip_data_loader.Run(rom_data.file_pos);
    kiwi::nes::Byte* zip_data = zip_data_container.data();
    size_t zip_size = zip_data_container.size();

    unzFile file =
        unzOpenFromMemory(const_cast<kiwi::nes::Byte*>(zip_data), zip_size);
    if (file) {
      kiwi::nes::Bytes manifest;
      // Loads title or i18 names on demand.
      std::map<std::string, std::unordered_map<std::string, std::string>>
          i18n_names;
      // Boxarts width and height
      std::map<std::string, std::pair<int, int>> boxarts_sizes;

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

        if (manifest_json.contains("boxarts")) {
          const auto& boxarts = manifest_json.at("boxarts");
          for (const auto& rom_version : boxarts.items()) {
            boxarts_sizes[rom_version.key()] = std::make_pair(
                rom_version.value()["width"], rom_version.value()["height"]);
          }
        }
      } else {
        // A zip file doesn't have a manifest should load image immediately, to
        // get its boxart size, and it will only has one rom.
        kiwi::nes::Bytes boxart_data =
            LoadPresetROM(rom_data, RomPart::kBoxArt);
        SDL_RWops* rw =
            SDL_RWFromConstMem(boxart_data.data(), boxart_data.size());
        SDL_Surface* surface = IMG_Load_RW(rw, true);
        rom_data.boxart_width = surface->w;
        rom_data.boxart_height = surface->h;
        SDL_FreeSurface(surface);
      }

      auto default_i18n_names = i18n_names.find("default");
      if (default_i18n_names != i18n_names.end()) {
        // Found the default name, which is the primary ROM's name.
        rom_data.i18n_names = default_i18n_names->second;
        rom_data.region = GuessROMRegion(rom_data.name);
      }
      rom_data.title_loaded = true;

      auto default_boxart_size = boxarts_sizes.find("default");
      if (default_boxart_size != boxarts_sizes.end()) {
        rom_data.boxart_width = std::get<0>(default_boxart_size->second);
        rom_data.boxart_height = std::get<1>(default_boxart_size->second);
      }

      if (!success) {
        unzClose(file);
        return;
      }

      // Find alternative roms.
      int located = unzGoToFirstFile(file);
      kiwi::base::FilePath alter_rom_path;
      std::string filename;
      filename.resize(kFileNameMaxLength);
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
          std::string alter_name =
              alter_rom_path.BaseName().RemoveExtension().AsUTF8Unsafe();

          // Finds corresponding i18n names, and store these names.
          auto alter_i18n_names = i18n_names.find(alter_name);
          std::unordered_map<std::string, std::string> names;
          if (alter_i18n_names != i18n_names.end()) {
            names = alter_i18n_names->second;
          }

          // Finds boxart image size
          auto alter_boxart_size = boxarts_sizes.find(alter_name);
          int alter_boxart_width = 0;
          int alter_boxart_height = 0;
          if (alter_boxart_size != boxarts_sizes.end()) {
            alter_boxart_width = std::get<0>(alter_boxart_size->second);
            alter_boxart_height = std::get<1>(alter_boxart_size->second);
          }

          if (alternative_rom_iter != rom_data.alternates.end()) {
            // Use the existing alternative rom struct.
            alternative_rom_iter->i18n_names = names;
            alternative_rom_iter->boxart_width = alter_boxart_width;
            alternative_rom_iter->boxart_height = alter_boxart_height;
          } else {
            preset_roms::PresetROM alternative_rom;
            alternative_rom.title_loaded = true;
            alternative_rom.file_pos = rom_data.file_pos;
            alternative_rom.zip_data_loader = rom_data.zip_data_loader;

            // Boxart image size
            alternative_rom.boxart_width = alter_boxart_width;
            alternative_rom.boxart_height = alter_boxart_height;

            // Leaky name
            std::string rom_name =
                alter_rom_path.RemoveExtension().BaseName().AsUTF8Unsafe();
            alternative_rom.name = new char[rom_name.size() + 1];
            strcpy(const_cast<char*>(alternative_rom.name), rom_name.c_str());
            alternative_rom.i18n_names = names;
            alternative_rom.region = GuessROMRegion(rom_name);
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

kiwi::nes::Bytes LoadPresetROM(const preset_roms::PresetROM& rom_data,
                               RomPart part) {
  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner =
      Application::Get()->GetIOTaskRunner();
  SDL_assert(io_task_runner->RunsTasksInCurrentSequence());

  kiwi::nes::Bytes result;
  kiwi::nes::Bytes zip_data_container =
      rom_data.zip_data_loader.Run(rom_data.file_pos);
  kiwi::nes::Byte* zip_data = zip_data_container.data();
  size_t zip_size = zip_data_container.size();

  unzFile file =
      unzOpenFromMemory(const_cast<kiwi::nes::Byte*>(zip_data), zip_size);
  if (file) {
    switch (part) {
      case RomPart::kBoxArt:
        if (!ReadFileFromZip(file, std::string(rom_data.name) + ".jpg",
                             result)) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to get boxart for name %s", rom_data.name);
        }
        break;
      case RomPart::kContent:
        if (!ReadFileFromZip(file, std::string(rom_data.name) + ".nes",
                             result)) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Failed to get rom data for name %s", rom_data.name);
        }
        break;
      default:
        break;
    }

    unzClose(file);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get file pointer");
  }
  return result;
}

// Reads all roms' data from package file.
void OpenRomDataFromPackage(std::vector<preset_roms::PresetROM>& roms,
                            std::map<std::string, std::string>& titles,
                            kiwi::nes::Bytes& icon,
                            kiwi::nes::Bytes& icon_highlight,
                            const kiwi::base::FilePath& package) {
  scoped_refptr<Unz> pak = OpenUnz(package);
  SDL_assert(pak);

  int located = unzGoToFirstFile(*pak);
  std::string filename;
  filename.resize(kFileNameMaxLength);
  while (located == UNZ_OK) {
    unz_file_info fi;
    unzGetCurrentFileInfo(*pak, &fi, filename.data(), filename.size(), nullptr,
                          0, nullptr, 0);

    if (strcmp(filename.data(), "manifest.json") == 0) {
      kiwi::nes::Bytes manifest_data;
      manifest_data.resize(fi.uncompressed_size);
      unzOpenCurrentFile(*pak);
      unzReadCurrentFile(*pak, manifest_data.data(), manifest_data.size());
      manifest_data.push_back(0);  // String terminator
      manifest_data.push_back(0);  // String terminator
      unzCloseCurrentFile(*pak);

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
      unzGoToNextFile(*pak);
      continue;
    }

    unzOpenCurrentFile(*pak);

    preset_roms::PresetROM rom;
    kiwi::base::FilePath filepath =
        kiwi::base::FilePath::FromUTF8Unsafe(filename.c_str());
    std::string name = filepath.RemoveExtension().AsUTF8Unsafe();

    unz_file_pos file_pos;
    unzGetFilePos(*pak, &file_pos);
    rom.file_pos = file_pos;
    rom.zip_data_loader = kiwi::base::BindRepeating(
        &LoadZipDataFromFilePos,
        // TODO RetainedRef will keep unzFile opened, especially in Android, it
        // will hold package's content. Try to cleanup these repeating callbacks
        // when it won't be used anymore.
        kiwi::base::RetainedRef(pak));

    rom.name = new char[name.size() + 1];
    strncpy(const_cast<char*>(rom.name), name.data(), name.size() + 1);
    unzCloseCurrentFile(*pak);
    roms.push_back(std::move(rom));
    located = unzGoToNextFile(*pak);
  }
}

preset_roms::Package* CreatePackageFromFile(
    const kiwi::base::FilePath& package_path) {
  std::vector<preset_roms::PresetROM> roms;
  std::map<std::string, std::string> titles;
  kiwi::nes::Bytes icon, icon_highlight;
  OpenRomDataFromPackage(roms, titles, icon, icon_highlight, package_path);
  preset_roms::Package* package =
      new PackageImpl(std::move(roms), std::move(titles), std::move(icon),
                      std::move(icon_highlight));
  return package;
}

void CloseRomDataFromPackage(preset_roms::PresetROM& rom) {
  delete[] rom.name;
}

void OpenPackageFromFile(const kiwi::base::FilePath& package_path) {
  g_packages.push_back(CreatePackageFromFile(package_path));
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
