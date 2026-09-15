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

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "nes/rom_hash.h"
#include "preset_roms/preset_roms.h"
#include "third_party/nlohmann_json/json.hpp"
#include "third_party/zlib-1.3.2/contrib/minizip/unzip.h"
#include "ui/application.h"
#include "utility/localization.h"
#include "utility/texture_parser/mesen_texture_parser.h"
#include "utility/texture_parser/texture_resource_provider.h"
#include "utility/texture_renderer.h"

namespace {
constexpr size_t kFileNameMaxLength = 256;

using TexturePackIndex = std::unordered_map<std::string, kiwi::base::FilePath>;

preset_roms::PresetROM* FindAlternateROMByName(
    std::vector<preset_roms::PresetROM>* alternates,
    std::string_view name) {
  for (preset_roms::PresetROM& alternate : *alternates) {
    if (name == kiwi::base::StringPiece(alternate.name)) {
      return &alternate;
    }
  }
  return nullptr;
}

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

bool GetCurrentZipFileName(unzFile file, std::string* filename) {
  unz_file_info file_info = {};
  if (unzGetCurrentFileInfo(file, &file_info, nullptr, 0, nullptr, 0, nullptr,
                            0) != UNZ_OK) {
    return false;
  }

  std::vector<char> filename_buffer(file_info.size_filename + 1, 0);
  if (unzGetCurrentFileInfo(file, &file_info, filename_buffer.data(),
                            filename_buffer.size(), nullptr, 0, nullptr,
                            0) != UNZ_OK) {
    return false;
  }
  *filename = filename_buffer.data();
  return true;
}

std::unordered_set<std::string> GetZipEntries(unzFile file) {
  std::unordered_set<std::string> entries;
  int located = unzGoToFirstFile(file);
  while (located == UNZ_OK) {
    std::string filename;
    if (!GetCurrentZipFileName(file, &filename)) {
      entries.clear();
      break;
    }
    std::replace(filename.begin(), filename.end(), '\\', '/');
    entries.insert(std::move(filename));
    located = unzGoToNextFile(file);
  }
  if (located != UNZ_END_OF_LIST_OF_FILE) {
    entries.clear();
  }
  return entries;
}

class ZipTextureResourceProvider final : public TextureResourceProvider {
 public:
  explicit ZipTextureResourceProvider(unzFile file)
      : file_(file), file_paths_(GetZipEntries(file)) {}
  ~ZipTextureResourceProvider() override = default;

  const std::unordered_set<std::string>& GetFilePaths() const override {
    return file_paths_;
  }

  std::optional<kiwi::nes::Bytes> ReadFile(std::string_view path) override {
    kiwi::nes::Bytes data;
    if (!ReadFileFromZip(file_, std::string(path), data)) {
      return std::nullopt;
    }
    return data;
  }

 private:
  unzFile file_;
  std::unordered_set<std::string> file_paths_;
};

std::vector<std::string> FindMesenHDTexturePackRoots(
    std::string_view archive_root,
    const std::unordered_set<std::string>& archive_entries) {
  constexpr std::string_view kHiresSuffix = "/hires.txt";
  const std::string path_prefix =
      archive_root.empty() ? "" : std::string(archive_root) + "/";
  std::vector<std::string> pack_roots;
  for (const std::string& entry : archive_entries) {
    if (entry.compare(0, path_prefix.size(), path_prefix) != 0) {
      continue;
    }
    if (entry == "hires.txt") {
      pack_roots.emplace_back();
    } else if (entry.size() >= kHiresSuffix.size() &&
               entry.compare(entry.size() - kHiresSuffix.size(),
                             kHiresSuffix.size(), kHiresSuffix) == 0) {
      pack_roots.push_back(entry.substr(0, entry.size() - kHiresSuffix.size()));
    }
  }
  std::sort(pack_roots.begin(), pack_roots.end());
  pack_roots.erase(std::unique(pack_roots.begin(), pack_roots.end()),
                   pack_roots.end());
  return pack_roots;
}

std::vector<std::unique_ptr<MesenTextureParser>> CreateMesenTextureParsers(
    TextureResourceProvider& resources,
    std::string archive_root) {
  std::replace(archive_root.begin(), archive_root.end(), '\\', '/');
  while (!archive_root.empty() &&
         (archive_root.back() == '/' || archive_root.back() == '\\')) {
    archive_root.pop_back();
  }

  const std::unordered_set<std::string>& archive_entries =
      resources.GetFilePaths();
  std::vector<std::unique_ptr<MesenTextureParser>> parsers;
  for (const std::string& pack_root :
       FindMesenHDTexturePackRoots(archive_root, archive_entries)) {
    std::optional<kiwi::nes::Bytes> hires_contents = resources.ReadFile(
        pack_root.empty() ? "hires.txt" : pack_root + "/hires.txt");
    if (!hires_contents) {
      continue;
    }
    const char* hires_data =
        hires_contents->empty()
            ? ""
            : reinterpret_cast<const char*>(hires_contents->data());
    parsers.push_back(std::make_unique<MesenTextureParser>(
        pack_root, std::string_view(hires_data, hires_contents->size()),
        archive_entries));
  }
  if (parsers.empty()) {
    return {};
  }
  return parsers;
}

std::unique_ptr<TextureParser> CreateTextureParser(
    TextureResourceProvider& resources,
    const nlohmann::json& manifest) {
  std::string texture_type;
  std::string archive_root;
  const auto standalone_type = manifest.find("type");
  if (standalone_type != manifest.end() && standalone_type->is_string()) {
    texture_type = standalone_type->get<std::string>();
  } else {
    const auto texture = manifest.find("texture");
    if (texture == manifest.end() || !texture->is_object()) {
      return nullptr;
    }
    const auto path = texture->find("path");
    const auto type = texture->find("type");
    if (path == texture->end() || !path->is_string() ||
        type == texture->end() || !type->is_string()) {
      return nullptr;
    }
    texture_type = type->get<std::string>();
    archive_root = path->get<std::string>();
  }
  if (texture_type != "mesen") {
    return nullptr;
  }

  std::vector<std::unique_ptr<MesenTextureParser>> parsers =
      CreateMesenTextureParsers(resources, std::move(archive_root));
  if (parsers.empty()) {
    return nullptr;
  }
  return std::make_unique<MesenTextureParserCollection>(std::move(parsers));
}

unzFile OpenUnzFromRWops(SDL_RWops* ops) {
  if (!ops) {
    return nullptr;
  }
  // Filling zlib_filefunc_def struct by SDL_RWops.
  zlib_filefunc_def func = {};
  func.zopen_file = OpenFileImpl;
  func.zread_file = ReadFileImpl;
  func.ztell_file = TellFileImpl;
  func.zseek_file = SeekFileImpl;
  func.zclose_file = CloseFileImpl;
  func.zerror_file = ErrorFileImpl;

  unzFile file = unzOpen2(reinterpret_cast<const char*>(ops), &func);
  return file;
}

unzFile unzOpenFromMemory(kiwi::nes::Byte* data, size_t size) {
  return OpenUnzFromRWops(
      SDL_RWFromMem(const_cast<kiwi::nes::Byte*>(data), size));
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
  // PAK assets are stored uncompressed in the APK and support random access.
  // Let minizip read them through SDL instead of copying the entire asset.
  return kiwi::base::MakeRefCounted<Unz>(
      OpenUnzFromRWops(SDL_RWFromFile(file.AsUTF8Unsafe().c_str(), "rb")));
#else
  return kiwi::base::MakeRefCounted<Unz>(unzOpen(file.AsUTF8Unsafe().c_str()));
#endif
}

scoped_refptr<Unz> OpenPresetROMArchive(
    const preset_roms::PresetROM& rom_data) {
  scoped_refptr<Unz> archive = kiwi::base::MakeRefCounted<Unz>(nullptr);
  archive->data = rom_data.zip_data_loader.Run(rom_data.file_pos);
  archive->unz = unzOpenFromMemory(archive->data.data(), archive->data.size());
  return archive;
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

std::optional<nlohmann::json> ReadTexturePackManifest(unzFile archive) {
  kiwi::nes::Bytes manifest;
  if (!ReadFileFromZip(archive, "manifest.json", manifest)) {
    return std::nullopt;
  }
  const char* data =
      manifest.empty() ? "" : reinterpret_cast<const char*>(manifest.data());
  nlohmann::json json =
      nlohmann::json::parse(data, data + manifest.size(), nullptr, false);
  if (json.is_discarded() || !json.is_object()) {
    return std::nullopt;
  }
  return json;
}

bool ConfigureTexturePackForROM(preset_roms::PresetROM* rom,
                                const kiwi::base::FilePath& texture_pack_path) {
  scoped_refptr<Unz> rom_archive = OpenPresetROMArchive(*rom);
  scoped_refptr<Unz> texture_archive = OpenUnz(texture_pack_path);
  if (!*rom_archive || !*texture_archive) {
    return false;
  }

  kiwi::nes::Bytes rom_data;
  if (!ReadFileFromZip(*rom_archive, std::string(rom->name) + ".nes",
                       rom_data)) {
    return false;
  }
  std::optional<nlohmann::json> manifest =
      ReadTexturePackManifest(*texture_archive);
  if (!manifest) {
    return false;
  }

  ZipTextureResourceProvider resources(*texture_archive);
  std::unique_ptr<TextureParser> parser =
      CreateTextureParser(resources, *manifest);
  if (!parser || !parser->Verify(rom_data)) {
    return false;
  }

  rom->hd_texture_path = texture_pack_path;
  rom->hd_edition_available = parser->HasRomPatch(rom_data);
  rom->hd_texture_toggle_available = !rom->hd_edition_available;
  return true;
}

void ConfigureTexturePackForROMFromIndex(
    preset_roms::PresetROM* rom,
    const TexturePackIndex& pack_by_rom_sha1) {
  const auto match = pack_by_rom_sha1.find(rom->sha1);
  if (match == pack_by_rom_sha1.end()) {
    return;
  }
  if (!ConfigureTexturePackForROM(rom, match->second)) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Texture package failed validation for ROM: %s", rom->name);
  }
}

}  // namespace

LoadedPresetROM::LoadedPresetROM() = default;
LoadedPresetROM::~LoadedPresetROM() = default;
LoadedPresetROM::LoadedPresetROM(LoadedPresetROM&&) noexcept = default;
LoadedPresetROM& LoadedPresetROM::operator=(LoadedPresetROM&&) noexcept =
    default;

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
        kiwi::nes::Bytes boxart_data = LoadPresetROMBoxArt(rom_data);
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
      while (located == UNZ_OK) {
        std::string filename;
        if (!GetCurrentZipFileName(file, &filename)) {
          break;
        }

        const kiwi::base::FilePath alter_rom_path =
            kiwi::base::FilePath::FromUTF8Unsafe(filename);
        const std::string extension =
            kiwi::base::FilePath(alter_rom_path.Extension()).AsUTF8Unsafe();
        if (kiwi::base::CompareCaseInsensitiveASCII(extension, ".nes") != 0) {
          located = unzGoToNextFile(file);
          continue;
        }

        const std::string alter_name =
            alter_rom_path.RemoveExtension().BaseName().AsUTF8Unsafe();
        std::string sha1;
        const auto indexed_sha1 = rom_data.sha1_by_name.find(alter_name);
        if (indexed_sha1 != rom_data.sha1_by_name.end()) {
          sha1 = indexed_sha1->second;
        } else {
          kiwi::nes::Bytes rom_contents;
          if (!ReadCurrentFileFromZip(file, rom_contents)) {
            located = unzGoToNextFile(file);
            continue;
          }
          sha1 = kiwi::nes::CalculateSha1Hex(rom_contents);
        }

        if (alter_name == kiwi::base::StringPiece(rom_data.name)) {
          rom_data.sha1 = sha1;
          located = unzGoToNextFile(file);
          continue;
        }

        preset_roms::PresetROM* alternative_rom =
            FindAlternateROMByName(&rom_data.alternates, alter_name);

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

        if (alternative_rom) {
          // Use the existing alternative rom struct.
          alternative_rom->i18n_names = names;
          alternative_rom->boxart_width = alter_boxart_width;
          alternative_rom->boxart_height = alter_boxart_height;
          alternative_rom->sha1 = sha1;
        } else {
          preset_roms::PresetROM new_alternative_rom;
          new_alternative_rom.title_loaded = true;
          new_alternative_rom.file_pos = rom_data.file_pos;
          new_alternative_rom.zip_data_loader = rom_data.zip_data_loader;
          new_alternative_rom.sha1 = sha1;

          // Boxart image size
          new_alternative_rom.boxart_width = alter_boxart_width;
          new_alternative_rom.boxart_height = alter_boxart_height;

          // Leaky name
          new_alternative_rom.name = new char[alter_name.size() + 1];
          strcpy(const_cast<char*>(new_alternative_rom.name),
                 alter_name.c_str());
          new_alternative_rom.i18n_names = names;
          new_alternative_rom.region = GuessROMRegion(alter_name);
          rom_data.alternates.push_back(std::move(new_alternative_rom));
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

void InitializeTexturePacks(
    const std::vector<kiwi::base::FilePath>& texture_pack_paths) {
  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner =
      Application::Get()->GetIOTaskRunner();
  SDL_assert(io_task_runner->RunsTasksInCurrentSequence());

  TexturePackIndex pack_by_rom_sha1;
  for (const kiwi::base::FilePath& path : texture_pack_paths) {
    scoped_refptr<Unz> archive = OpenUnz(path);
    if (!*archive) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Failed to open texture package: %s",
                  path.AsUTF8Unsafe().c_str());
      continue;
    }
    std::optional<nlohmann::json> manifest = ReadTexturePackManifest(*archive);
    if (!manifest) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Invalid texture package manifest: %s",
                  path.AsUTF8Unsafe().c_str());
      continue;
    }
    const auto type = manifest->find("type");
    if (type == manifest->end() || !type->is_string() ||
        type->get<std::string>() != "mesen") {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Invalid texture package type: %s",
                  path.AsUTF8Unsafe().c_str());
      continue;
    }
    const auto roms = manifest->find("roms");
    if (roms == manifest->end() || !roms->is_array()) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Texture package manifest has no ROM list: %s",
                  path.AsUTF8Unsafe().c_str());
      continue;
    }

    for (const auto& value : *roms) {
      if (!value.is_string()) {
        continue;
      }
      std::optional<std::string> sha1 =
          kiwi::nes::NormalizeSha1Hex(value.get<std::string>());
      if (!sha1) {
        continue;
      }
      const auto [existing, inserted] = pack_by_rom_sha1.emplace(*sha1, path);
      if (!inserted && existing->second != path) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Multiple texture packages target ROM SHA-1 %s",
                    sha1->c_str());
      }
    }
  }

  for (preset_roms::Package* package :
       preset_roms::GetPresetOrTestRomsPackages()) {
    for (size_t i = 0; i < package->GetRomsCount(); ++i) {
      preset_roms::PresetROM& rom = package->GetRomsByIndex(i);
      ConfigureTexturePackForROMFromIndex(&rom, pack_by_rom_sha1);
      for (preset_roms::PresetROM& alternative : rom.alternates) {
        ConfigureTexturePackForROMFromIndex(&alternative, pack_by_rom_sha1);
      }
    }
  }
}

kiwi::nes::Bytes LoadPresetROMBoxArt(const preset_roms::PresetROM& rom_data) {
  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner =
      Application::Get()->GetIOTaskRunner();
  SDL_assert(io_task_runner->RunsTasksInCurrentSequence());

  kiwi::nes::Bytes result;
  scoped_refptr<Unz> archive = OpenPresetROMArchive(rom_data);
  if (!*archive) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get file pointer");
    return result;
  }

  if (!ReadFileFromZip(*archive, std::string(rom_data.name) + ".jpg", result)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get boxart for name %s", rom_data.name);
  }
  return result;
}

LoadedPresetROM LoadPresetROM(const preset_roms::PresetROM& rom_data,
                              preset_roms::ROMEdition edition) {
  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner =
      Application::Get()->GetIOTaskRunner();
  SDL_assert(io_task_runner->RunsTasksInCurrentSequence());

  LoadedPresetROM result;
  scoped_refptr<Unz> archive = OpenPresetROMArchive(rom_data);
  if (!*archive) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get file pointer");
    return result;
  }

  if (!ReadFileFromZip(*archive, std::string(rom_data.name) + ".nes",
                       result.rom_data)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to get rom data for name %s", rom_data.name);
    return result;
  }

  const bool should_load_texture = !rom_data.hd_texture_path.empty() &&
                                   (rom_data.hd_texture_toggle_available ||
                                    edition == preset_roms::ROMEdition::kHD);
  if (should_load_texture) {
    scoped_refptr<Unz> texture_archive = OpenUnz(rom_data.hd_texture_path);
    std::optional<nlohmann::json> manifest =
        *texture_archive ? ReadTexturePackManifest(*texture_archive)
                         : std::nullopt;
    if (manifest) {
      ZipTextureResourceProvider resources(*texture_archive);
      std::unique_ptr<TextureParser> parser =
          CreateTextureParser(resources, *manifest);
      if (parser && parser->Verify(result.rom_data)) {
        const bool rom_patch_required = parser->HasRomPatch(result.rom_data);
        result.texture_renderer =
            parser->CreateTextureRenderer(result.rom_data, resources);

        if (result.texture_renderer && rom_patch_required) {
          // Keep the patched bytes as the emulator input. Its resulting CRC
          // intentionally gives non-switchable editions separate saves.
          if (!parser->ApplyRomPatch(&result.rom_data, resources)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Failed to apply HD texture ROM patch for name %s",
                         rom_data.name);
            result.texture_renderer.reset();
          }
        } else if (result.texture_renderer) {
          result.hd_texture_toggle_available = true;
        }
      }
    }
  }

  if (!result.texture_renderer && edition == preset_roms::ROMEdition::kHD) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to load HD texture data for name %s", rom_data.name);
    result.rom_data.clear();
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

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      rom_sha1s;
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
      const auto sha1_index = manifest_json.find("rom_sha1s");
      if (sha1_index != manifest_json.end() && sha1_index->is_object()) {
        for (const auto& package_hashes : sha1_index->items()) {
          if (!package_hashes.value().is_object()) {
            continue;
          }
          for (const auto& rom_hash : package_hashes.value().items()) {
            if (rom_hash.value().is_string()) {
              std::optional<std::string> sha1 = kiwi::nes::NormalizeSha1Hex(
                  rom_hash.value().get<std::string>());
              if (sha1) {
                rom_sha1s[package_hashes.key()][rom_hash.key()] =
                    std::move(*sha1);
              }
            }
          }
        }
      }
      located = unzGoToNextFile(*pak);
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

  for (preset_roms::PresetROM& rom : roms) {
    const auto hashes = rom_sha1s.find(rom.name);
    if (hashes != rom_sha1s.end()) {
      rom.sha1_by_name = hashes->second;
    }
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
