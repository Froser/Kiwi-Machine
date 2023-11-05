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
#include "third_party/zlib-1.3/contrib/minizip/unzip.h"

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
  if (err != UNZ_OK) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Can't read file of %s",
                name.c_str());
    return false;
  }

  return ReadCurrentFileFromZip(file, data);
}

bool ReadRomAndCover(unzFile file,
                     const char* name,
                     kiwi::nes::Bytes& rom,
                     kiwi::nes::Bytes& cover) {
  return ReadFileFromZip(file, std::string(name) + ".nes", rom) &&
         ReadFileFromZip(file, std::string(name) + ".jpg", cover);
}

void ReadNESOrCover(unzFile file,
                    const kiwi::base::FilePath& rom_path,
                    preset_roms::PresetROM& out) {
  if (rom_path.FinalExtension() == FILE_PATH_LITERAL(".nes"))
    ReadCurrentFileFromZip(file, out.rom_data);
  else if (rom_path.FinalExtension() == FILE_PATH_LITERAL(".jpg"))
    ReadCurrentFileFromZip(file, out.rom_cover);
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

}  // namespace

void FillRomDataFromZip(const preset_roms::PresetROM& rom_data) {
  unzFile file = unzOpenFromMemory(
      const_cast<kiwi::nes::Byte*>(rom_data.zip_data), rom_data.zip_size);
  if (file) {
    bool success = ReadRomAndCover(file, rom_data.name, rom_data.rom_data,
                                   rom_data.rom_cover);
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

      if (alter_rom_path.RemoveExtension().BaseName() ==
          kiwi::base::StringPiece(rom_data.name)) {
        located = unzGoToNextFile(file);
        continue;
      }

      auto alternative_rom_iter =
          std::find_if(rom_data.alternates.begin(), rom_data.alternates.end(),
                       [&alter_rom_path](const preset_roms::PresetROM& lhs) {
                         return alter_rom_path.RemoveExtension().BaseName() ==
                                kiwi::base::StringPiece(lhs.name);
                       });

      if (alternative_rom_iter != rom_data.alternates.end()) {
        // Use the existing alternative rom struct.
        ReadNESOrCover(file, alter_rom_path, *alternative_rom_iter);
      } else {
        preset_roms::PresetROM alternative_rom;
        // Leaky name
        std::string rom_name =
            alter_rom_path.RemoveExtension().BaseName().AsUTF8Unsafe();
        alternative_rom.name = new char[rom_name.size() + 1];
        strcpy(const_cast<char*>(alternative_rom.name), rom_name.c_str());
        ReadNESOrCover(file, alter_rom_path, alternative_rom);
        rom_data.alternates.push_back(std::move(alternative_rom));
      }

      located = unzGoToNextFile(file);
    }

    unzClose(file);
  } else {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Can't load rom zip data of name %s", rom_data.name);
  }
}

#if defined(KIWI_USE_EXTERNAL_PAK)

// Reads all roms' data from package file.
void OpenRomDataFromPackage(std::vector<preset_roms::PresetROM>& roms,
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
    unzOpenCurrentFile(pak);

    preset_roms::PresetROM rom;
    kiwi::base::FilePath filepath =
        kiwi::base::FilePath::FromUTF8Unsafe(filename.c_str());
    std::string name = filepath.RemoveExtension().AsUTF8Unsafe();

    rom.name = new char[name.size() + 1];
    strncpy(const_cast<char*>(rom.name), name.data(), name.size() + 1);
    rom.zip_size = fi.uncompressed_size;
    rom.zip_data = new kiwi::nes::Byte[rom.zip_size];
    unzReadCurrentFile(pak, const_cast<kiwi::nes::Byte*>(rom.zip_data),
                       rom.zip_size);
    unzCloseCurrentFile(pak);
    roms.push_back(std::move(rom));
    located = unzGoToNextFile(pak);
  }

  unzClose(pak);
}

void CloseRomDataFromPackage(std::vector<preset_roms::PresetROM>& roms) {
  for (auto& rom : roms) {
    delete[] rom.name;
    delete[] rom.zip_data;
  }
}
#endif