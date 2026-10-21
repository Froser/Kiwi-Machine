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

#include "models/nes/runtime.h"
#include "build/build_config.h"

#include <SDL.h>
#include <SDL_image.h>
#include <time.h>
#include <tiny_jpeg.h>
#include <chrono>
#include <memory>
#include <set>
#include <vector>

#include "emulator/nes/rom_hash.h"
#include "models/battery_save.h"
#include "ui/application.h"
#include "ui/widgets/canvas.h"

#if BUILDFLAG(IS_WASM)
#include "utility/emscripten/bridge_api.h"
#endif

namespace {
using WeakLifetime = std::weak_ptr<int>;

struct FilePathSorter {
  bool operator()(const kiwi::base::FilePath& lhs,
                  const kiwi::base::FilePath& rhs) const {
    uint64_t timestamp_lhs;
    kiwi::base::StringToUint64(lhs.BaseName().AsUTF8Unsafe(), &timestamp_lhs);

    uint64_t timestamp_rhs;
    kiwi::base::StringToUint64(rhs.BaseName().AsUTF8Unsafe(), &timestamp_rhs);

    return timestamp_lhs > timestamp_rhs;
  }
};

kiwi::base::FilePath GetStatesPath(const kiwi::base::FilePath& profile_path,
                                   int crc32) {
  kiwi::base::FilePath path_to_states =
      profile_path.Append(FILE_PATH_LITERAL("nes"))
          .Append(FILE_PATH_LITERAL("States"));
  path_to_states = path_to_states.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(kiwi::base::NumberToString(crc32)));
  return path_to_states;
}

kiwi::base::FilePath GetAutoSavedStatePath(
    const kiwi::base::FilePath& profile_path,
    int crc32) {
  kiwi::base::FilePath auto_saved_snapshot_path =
      GetStatesPath(profile_path, crc32);
  auto_saved_snapshot_path =
      auto_saved_snapshot_path.Append(FILE_PATH_LITERAL("AutoSaved"));
  return auto_saved_snapshot_path;
}

// Layout of a NESRuntime snapshot:
// ./nes/States/{CRC}/{Slot}/data
// ./nes/States/{CRC}/{Slot}/thumbnail
kiwi::base::FilePath GetSnapshotPath(const kiwi::base::FilePath& profile_path,
                                     int crc32,
                                     int slot) {
  kiwi::base::FilePath path_to_snapshot = GetStatesPath(profile_path, crc32);
  path_to_snapshot = path_to_snapshot.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(kiwi::base::NumberToString(slot)));
  return path_to_snapshot;
}

kiwi::base::FilePath GetSnapshotDataPath(const kiwi::base::FilePath& path) {
  return path.Append(FILE_PATH_LITERAL("data"));
}

kiwi::base::FilePath GetSnapshotThumbnailPath(
    const kiwi::base::FilePath& path) {
  return path.Append(FILE_PATH_LITERAL("thumbnail"));
}

kiwi::base::FilePath GetSnapshotDataPath(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    int slot) {
  return GetSnapshotDataPath(GetSnapshotPath(profile_path, crc32, slot));
}

kiwi::base::FilePath GetSnapshotThumbnailPath(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    int slot) {
  return GetSnapshotThumbnailPath(GetSnapshotPath(profile_path, crc32, slot));
}

bool SaveStateByPathOnIOThread(
    const kiwi::base::FilePath& path_to_data,
    const kiwi::base::FilePath& path_to_thumbnail,
    const kiwi::nes::Bytes& state_data,
    const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail_data) {
  if (!kiwi::base::PathExists(path_to_data.DirName()))
    kiwi::base::CreateDirectory(path_to_data.DirName());

  {
    kiwi::base::File file(path_to_data, kiwi::base::File::FLAG_CREATE |
                                            kiwi::base::File::FLAG_WRITE);
    if (!file.IsValid())
      return false;

    file.Write(0, reinterpret_cast<const char*>(state_data.data()),
               state_data.size() * sizeof(state_data[0]));
  }

  {
    kiwi::base::File file(path_to_thumbnail, kiwi::base::File::FLAG_CREATE |
                                                 kiwi::base::File::FLAG_WRITE);
    if (!file.IsValid())
      return false;

    // Transfer thumbnail data to jpeg to save space.
    tje_encode_with_func(
        [](void* context, void* data, int size) {
          kiwi::base::File* file = reinterpret_cast<kiwi::base::File*>(context);
          file->WriteAtCurrentPos(reinterpret_cast<const char*>(data), size);
        },
        &file, 2, Canvas::kNESFrameDefaultWidth, Canvas::kNESFrameDefaultHeight,
        4, reinterpret_cast<const unsigned char*>(thumbnail_data.data()),
        Canvas::kNESFrameDefaultWidth * sizeof(thumbnail_data[0]));
  }

#if BUILDFLAG(IS_WASM)
  SyncFilesystem();
#endif

  return true;
}

bool SaveStateOnIOThread(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    int slot,
    const kiwi::nes::Bytes& state_data,
    const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail_data) {
  kiwi::base::FilePath path_to_data =
      GetSnapshotDataPath(profile_path, crc32, slot);
  kiwi::base::FilePath path_to_thumbnail =
      GetSnapshotThumbnailPath(profile_path, crc32, slot);

  return SaveStateByPathOnIOThread(path_to_data, path_to_thumbnail, state_data,
                                   thumbnail_data);
}

bool SaveAutoSavedStateOnIOThread(
    time_t timestamp,
    const kiwi::base::FilePath& profile_path,
    int crc32,
    const kiwi::nes::Bytes& state_data,
    const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail_data) {
  kiwi::base::FilePath auto_saved_snapshot_path =
      GetAutoSavedStatePath(profile_path, crc32);
  std::string timestamp_str = std::to_string(timestamp);
  kiwi::base::FilePath state_path = auto_saved_snapshot_path.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(timestamp_str));
  bool success = SaveStateByPathOnIOThread(GetSnapshotDataPath(state_path),
                                           GetSnapshotThumbnailPath(state_path),
                                           state_data, thumbnail_data);

  if (success) {
    // Check all auto save states, delete redundant states.
    kiwi::base::FileEnumerator fe(auto_saved_snapshot_path, false,
                                  kiwi::base::FileEnumerator::DIRECTORIES);
    kiwi::base::FilePath path = fe.Next();
    std::set<kiwi::base::FilePath, FilePathSorter> files;
    while (!path.empty()) {
      // Sorted by base name (to utc)
      files.insert(path);
      path = fe.Next();
    }

    int max_auto_save_states = NESRuntime::MaxAutoSaveStates;
    while (files.size() > max_auto_save_states) {
      bool deleted = kiwi::base::DeletePathRecursively(*files.rbegin());
      SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Delete %s %s.",
                   files.begin()->BaseName().AsUTF8Unsafe().c_str(),
                   deleted ? "successfully" : "failed");
      if (!deleted) {
        // Increase max states count, the deletion failed entry will be deleted
        // next time.
        ++max_auto_save_states;
      }
      files.erase(*files.rbegin());
    }
  }

  return success;
}

int GetSavedStatesCountOnIOThread(const kiwi::base::FilePath& profile_path,
                                  int crc32) {
  int count = 0;
  for (int slot = 0; slot < NESRuntime::MaxSaveStates; ++slot) {
    if (kiwi::base::PathExists(
            GetSnapshotDataPath(profile_path, crc32, slot)) &&
        kiwi::base::PathExists(
            GetSnapshotThumbnailPath(profile_path, crc32, slot))) {
      ++count;
    }
  }
  return count;
}

int GetAutoSavedStatesCountOnIOThread(const kiwi::base::FilePath& profile_path,
                                      int crc32) {
  kiwi::base::FilePath auto_saved_snapshot_path =
      GetAutoSavedStatePath(profile_path, crc32);
  if (!kiwi::base::PathExists(auto_saved_snapshot_path))
    kiwi::base::CreateDirectory(auto_saved_snapshot_path);

  kiwi::base::FileEnumerator fe(auto_saved_snapshot_path, false,
                                kiwi::base::FileEnumerator::DIRECTORIES);
  kiwi::base::FilePath path = fe.Next();
  std::set<kiwi::base::FilePath, FilePathSorter> files;
  int count = 0;
  while (!path.empty()) {
    // Sorted by base name (to utc)
    files.insert(path);
    path = fe.Next();
    ++count;
  }
  return count;
}

kiwi::nes::Bytes ReadDataFromProfile(const std::string& path) {
  kiwi::nes::Bytes data;
  kiwi::base::File file(kiwi::base::FilePath::FromUTF8Unsafe(path),
                        kiwi::base::File::FLAG_READ);
  if (!file.IsValid())
    return data;

  data.resize(file.GetLength());
  file.Read(0, reinterpret_cast<char*>(data.data()), data.size());
  return data;
}

NESRuntime::StateResult GetStateByPathOnIOThread(
    const kiwi::base::FilePath& path_to_data,
    const kiwi::base::FilePath& path_to_thumbnail) {
  NESRuntime::StateResult sr;
  sr.state_data = ReadDataFromProfile(path_to_data.AsUTF8Unsafe());

  // Thumbnail is JPEG format, convert it to pixel data:
  kiwi::nes::Bytes source =
      ReadDataFromProfile(path_to_thumbnail.AsUTF8Unsafe());
  if (!source.empty()) {
    SDL_RWops* rw = SDL_RWFromConstMem(source.data(), source.size());
    SDL_Surface* surface = IMG_Load_RW(rw, true);
    if (surface) {
      SDL_Surface* converted =
          SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
      SDL_FreeSurface(surface);
      if (converted) {
        sr.thumbnail_width = converted->w;
        sr.thumbnail_height = converted->h;
        constexpr size_t kBytesPerPixel = sizeof(kiwi::emu::Color);
        const size_t row_size =
            static_cast<size_t>(converted->w) * kBytesPerPixel;
        sr.thumbnail_data.resize(row_size * converted->h);
        for (int row = 0; row < converted->h; ++row) {
          memcpy(sr.thumbnail_data.data() + row * row_size,
                 static_cast<const uint8_t*>(converted->pixels) +
                     row * converted->pitch,
                 row_size);
        }
        SDL_FreeSurface(converted);
      }
    }
  }

  sr.success = !sr.state_data.empty() && !sr.thumbnail_data.empty();
  return sr;
}

NESRuntime::StateResult GetStateOnIOThread(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    int slot) {
  kiwi::base::FilePath path_to_data =
      GetSnapshotDataPath(profile_path, crc32, slot);
  kiwi::base::FilePath path_to_thumbnail =
      GetSnapshotThumbnailPath(profile_path, crc32, slot);
  NESRuntime::StateResult sr =
      GetStateByPathOnIOThread(path_to_data, path_to_thumbnail);
  sr.slot_or_timestamp = slot;
  return sr;
}

NESRuntime::StateResult GetAutoSavedStateByTimestampOnIOThread(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    uint64_t timestamp) {
  kiwi::base::FilePath auto_saved_snapshot_path =
      GetAutoSavedStatePath(profile_path, crc32);
  kiwi::base::FilePath state_path = auto_saved_snapshot_path.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(std::to_string(timestamp)));

  NESRuntime::StateResult sr = GetStateByPathOnIOThread(
      GetSnapshotDataPath(state_path), GetSnapshotThumbnailPath(state_path));
  sr.slot_or_timestamp = timestamp;
  return sr;
}

bool DeleteAutoSavedStatesNewerThanOnIOThread(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    uint64_t timestamp) {
  const kiwi::base::FilePath auto_saved_snapshot_path =
      GetAutoSavedStatePath(profile_path, crc32);
  if (!kiwi::base::PathExists(auto_saved_snapshot_path))
    return true;

  std::vector<kiwi::base::FilePath> paths_to_delete;
  kiwi::base::FileEnumerator enumerator(
      auto_saved_snapshot_path, false, kiwi::base::FileEnumerator::DIRECTORIES);
  for (kiwi::base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    uint64_t saved_timestamp = 0;
    if (kiwi::base::StringToUint64(path.BaseName().AsUTF8Unsafe(),
                                   &saved_timestamp) &&
        saved_timestamp > timestamp) {
      paths_to_delete.push_back(path);
    }
  }

  bool success = true;
  for (const kiwi::base::FilePath& path : paths_to_delete)
    success = kiwi::base::DeletePathRecursively(path) && success;
  return success;
}

NESRuntime::StateResult GetAutoSavedStateOnIOThread(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    int slot) {
  NESRuntime::StateResult sr;
  kiwi::base::FilePath auto_saved_snapshot_path =
      GetAutoSavedStatePath(profile_path, crc32);
  if (!kiwi::base::PathExists(auto_saved_snapshot_path)) {
    bool created = kiwi::base::CreateDirectory(auto_saved_snapshot_path);
    if (!created) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Can't open or create auto saved entry at %s",
                   profile_path.AsUTF8Unsafe().c_str());
      return sr;
    }
  }

  kiwi::base::FileEnumerator fe(auto_saved_snapshot_path, false,
                                kiwi::base::FileEnumerator::DIRECTORIES);
  kiwi::base::FilePath path = fe.Next();
  std::set<kiwi::base::FilePath, FilePathSorter> files;
  while (!path.empty()) {
    // Sorted by base name (to utc)
    files.insert(path);
    path = fe.Next();
  }

  std::vector<kiwi::base::FilePath> random_files(files.begin(), files.end());
  if (random_files.empty())
    return sr;

  // Gets state file from the end of the vector.
  int index = slot < 0 ? (static_cast<int>(random_files.size()) + slot - 1)
                       : slot % random_files.size();
  if (index < 0)
    index = 0;
  SDL_assert(index >= 0 && index < random_files.size());
  const kiwi::base::FilePath& target_file = random_files[index];
  sr = GetStateByPathOnIOThread(GetSnapshotDataPath(target_file),
                                GetSnapshotThumbnailPath(target_file));
  uint64_t timestamp;
  kiwi::base::StringToUint64(target_file.BaseName().AsUTF8Unsafe(), &timestamp);
  sr.slot_or_timestamp = timestamp;
  return sr;
}

void OnBatterySaveWritten(WeakLifetime weak_lifetime,
                          NESRuntime* runtime_data,
                          std::string rom_sha1,
                          uint64_t generation,
                          kiwi::nes::Emulator::LoadCallback callback,
                          bool success) {
  if (weak_lifetime.expired()) {
    return;
  }
  if (!success) {
    std::move(callback).Run(false);
    return;
  }
  runtime_data->emulator()->AsNESEmulator()->AcknowledgePRGNVRAMSaved(
      rom_sha1, generation, std::move(callback));
}

bool WriteBatterySave(
    const kiwi::base::FilePath& profile_path,
    const std::optional<kiwi::base::FilePath>& explicit_save_path,
    const std::string& rom_sha1,
    const kiwi::nes::Bytes& data) {
  return explicit_save_path
             ? battery_save::WriteToPath(*explicit_save_path, data)
             : battery_save::Write(profile_path, rom_sha1, data);
}

void OnPRGNVRAMExported(
    WeakLifetime weak_lifetime,
    NESRuntime* runtime_data,
    std::optional<kiwi::base::FilePath> explicit_save_path,
    kiwi::nes::Emulator::LoadCallback callback,
    std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot) {
  if (weak_lifetime.expired()) {
    return;
  }
  if (!snapshot) {
    std::move(callback).Run(true);
    return;
  }

  const std::string rom_sha1 = snapshot->rom_sha1;
  const uint64_t generation = snapshot->generation;
  runtime_data->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&WriteBatterySave, runtime_data->profile_path(),
                           std::move(explicit_save_path), rom_sha1,
                           std::move(snapshot->data)),
      kiwi::base::BindOnce(&OnBatterySaveWritten, weak_lifetime, runtime_data,
                           rom_sha1, generation, std::move(callback)));
}

void ResumeOldROMAfterFailure(NESRuntime* runtime_data, bool resume_old_rom) {
  if (resume_old_rom) {
    runtime_data->emulator()->Run();
  }
}

struct PreparedROM {
  kiwi::nes::Bytes data;
  kiwi::nes::Emulator::LoadOptions options;
  std::optional<kiwi::nes::Bytes> initial_prg_nvram;
  bool retry_without_prg_nvram = false;
  std::optional<kiwi::base::FilePath> invalid_save_path;
};

std::optional<PreparedROM> PrepareROMData(
    const kiwi::base::FilePath& profile_path,
    kiwi::nes::Bytes data,
    kiwi::nes::Emulator::LoadOptions options,
    std::optional<kiwi::base::FilePath> save_path) {
  PreparedROM prepared;
  prepared.options = std::move(options);
  if (save_path) {
    battery_save::ReadResult save = battery_save::ReadFromPath(*save_path);
    if (!save.success || !save.exists) {
      return std::nullopt;
    }
    prepared.initial_prg_nvram = std::move(save.data);
  } else {
    std::optional<std::string> rom_sha1 =
        kiwi::nes::CalculateINESBatterySaveSha1Hex(data);
    if (rom_sha1) {
      std::optional<kiwi::base::FilePath> automatic_save_path =
          battery_save::GetPath(profile_path, *rom_sha1);
      battery_save::ReadResult save =
          automatic_save_path ? battery_save::ReadFromPath(*automatic_save_path)
                              : battery_save::ReadResult{};
      if (save.exists && !save.success) {
        std::optional<kiwi::base::FilePath> backup_path =
            battery_save::BackupInvalid(*automatic_save_path);
        if (!backup_path) {
          SDL_LogError(
              SDL_LOG_CATEGORY_APPLICATION,
              "Could not back up unreadable battery-backed save data.");
          return std::nullopt;
        }
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Ignoring unreadable battery-backed save data. Backup: %s",
                    backup_path->AsUTF8Unsafe().c_str());
      }
      if (save.exists && save.success) {
        prepared.initial_prg_nvram = std::move(save.data);
        prepared.retry_without_prg_nvram = true;
        prepared.invalid_save_path = std::move(automatic_save_path);
      }
    }
  }

  prepared.data = std::move(data);
  return prepared;
}

void OnPreparedROMLoaded(NESRuntime* runtime_data,
                         WeakLifetime weak_lifetime,
                         std::shared_ptr<const kiwi::nes::Bytes> rom_data,
                         kiwi::nes::Emulator::LoadOptions options,
                         bool retry_without_prg_nvram,
                         std::optional<kiwi::base::FilePath> invalid_save_path,
                         bool resume_old_rom,
                         kiwi::nes::Emulator::LoadCallback callback,
                         bool success);

void OnInvalidBatterySaveBackedUp(
    WeakLifetime weak_lifetime,
    NESRuntime* runtime_data,
    std::shared_ptr<const kiwi::nes::Bytes> rom_data,
    kiwi::nes::Emulator::LoadOptions options,
    bool resume_old_rom,
    kiwi::nes::Emulator::LoadCallback callback,
    std::optional<kiwi::base::FilePath> backup_path) {
  if (weak_lifetime.expired()) {
    return;
  }
  if (!backup_path) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Could not back up invalid battery-backed save data.");
    ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
    std::move(callback).Run(false);
    return;
  }

  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
              "Ignoring invalid battery-backed save data. Backup: %s",
              backup_path->AsUTF8Unsafe().c_str());
  runtime_data->emulator()->AsNESEmulator()->LoadAndRun(
      *rom_data,
      kiwi::base::BindOnce(&OnPreparedROMLoaded, runtime_data, weak_lifetime,
                           rom_data, options, false,
                           std::optional<kiwi::base::FilePath>(),
                           resume_old_rom, std::move(callback)),
      options);
}

void OnPreparedROMLoaded(NESRuntime* runtime_data,
                         WeakLifetime weak_lifetime,
                         std::shared_ptr<const kiwi::nes::Bytes> rom_data,
                         kiwi::nes::Emulator::LoadOptions options,
                         bool retry_without_prg_nvram,
                         std::optional<kiwi::base::FilePath> invalid_save_path,
                         bool resume_old_rom,
                         kiwi::nes::Emulator::LoadCallback callback,
                         bool success) {
  if (weak_lifetime.expired()) {
    return;
  }
  if (success) {
    std::move(callback).Run(true);
    return;
  }
  if (retry_without_prg_nvram && invalid_save_path) {
    runtime_data->GetIOTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        kiwi::base::BindOnce(&battery_save::BackupInvalid, *invalid_save_path),
        kiwi::base::BindOnce(&OnInvalidBatterySaveBackedUp, weak_lifetime,
                             runtime_data, rom_data, options, resume_old_rom,
                             std::move(callback)));
    return;
  }

  ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
  std::move(callback).Run(false);
}

void LoadPreparedROM(WeakLifetime weak_lifetime,
                     NESRuntime* runtime_data,
                     bool resume_old_rom,
                     kiwi::nes::Emulator::LoadCallback callback,
                     std::optional<PreparedROM> prepared) {
  if (weak_lifetime.expired()) {
    return;
  }
  if (!prepared) {
    ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
    std::move(callback).Run(false);
    return;
  }

  auto rom_data =
      std::make_shared<const kiwi::nes::Bytes>(std::move(prepared->data));
  kiwi::nes::Emulator::LoadOptions options = std::move(prepared->options);
  kiwi::nes::Emulator::LoadCallback load_callback = kiwi::base::BindOnce(
      &OnPreparedROMLoaded, runtime_data, weak_lifetime, rom_data, options,
      prepared->retry_without_prg_nvram, std::move(prepared->invalid_save_path),
      resume_old_rom, std::move(callback));
  if (prepared->initial_prg_nvram) {
    runtime_data->emulator()->AsNESEmulator()->LoadAndRunWithPRGNVRAM(
        *rom_data, *prepared->initial_prg_nvram, std::move(load_callback),
        options);
  } else {
    runtime_data->emulator()->AsNESEmulator()->LoadAndRun(
        *rom_data, std::move(load_callback), options);
  }
}

void OnCurrentROMSavedForLoad(WeakLifetime weak_lifetime,
                              NESRuntime* runtime_data,
                              kiwi::nes::Bytes rom_data,
                              kiwi::nes::Emulator::LoadOptions options,
                              std::optional<kiwi::base::FilePath> save_path,
                              bool resume_old_rom,
                              kiwi::nes::Emulator::LoadCallback callback,
                              bool success) {
  if (weak_lifetime.expired()) {
    return;
  }
  if (!success) {
    ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
    std::move(callback).Run(false);
    return;
  }

  runtime_data->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&PrepareROMData, runtime_data->profile_path(),
                           std::move(rom_data), std::move(options),
                           std::move(save_path)),
      kiwi::base::BindOnce(&LoadPreparedROM, weak_lifetime, runtime_data,
                           resume_old_rom, std::move(callback)));
}

void OnCurrentROMSavedForUnload(WeakLifetime weak_lifetime,
                                NESRuntime* runtime_data,
                                bool resume_old_rom,
                                kiwi::nes::Emulator::LoadCallback callback,
                                bool success) {
  if (weak_lifetime.expired()) {
    return;
  }
  if (!success) {
    ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
    std::move(callback).Run(false);
    return;
  }
  runtime_data->emulator()->AsNESEmulator()->Unload(kiwi::base::BindOnce(
      [](kiwi::nes::Emulator::LoadCallback callback) {
        std::move(callback).Run(true);
      },
      std::move(callback)));
}

}  // namespace

NESRuntime::NESRuntime(
    const kiwi::base::FilePath& profile_path,
    scoped_refptr<kiwi::nes::Emulator> emulator,
    scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<kiwi::base::SequencedTaskRunner> timer_task_runner)
    : Runtime(profile_path, std::move(timer_task_runner)),
      emulator_(emulator ? std::move(emulator) : kiwi::nes::CreateEmulator()),
      debug_port_(std::make_unique<DebugPort>(emulator_.get())),
      io_task_runner_(std::move(io_task_runner)) {}

NESRuntime::~NESRuntime() {
  Shutdown();
}

scoped_refptr<kiwi::emu::Emulator> NESRuntime::emulator() {
  return emulator_;
}

kiwi::nes::Emulator* NESRuntime::nes_emulator() {
  return emulator_.get();
}

const kiwi::nes::Emulator* NESRuntime::nes_emulator() const {
  return emulator_.get();
}

scoped_refptr<kiwi::base::SequencedTaskRunner> NESRuntime::GetIOTaskRunner() {
  return io_task_runner_ ? io_task_runner_
                         : Application::Get()->GetIOTaskRunner();
}

void NESRuntime::PersistState(int slot,
                              const kiwi::emu::Bytes& state_data,
                              const kiwi::emu::Colors& thumbnail,
                              kiwi::base::OnceCallback<void(bool)> callback) {
  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!rom_data) {
    std::move(callback).Run(false);
    return;
  }
  SaveState(rom_data->crc, slot, state_data, thumbnail, std::move(callback));
}

void NESRuntime::GetSavedStatesCount(
    kiwi::base::OnceCallback<void(int)> callback) {
  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!rom_data) {
    std::move(callback).Run(0);
    return;
  }
  GetSavedStatesCount(rom_data->crc, std::move(callback));
}

void NESRuntime::GetState(int slot, StateResultCallback callback) {
  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!rom_data) {
    StateResult result;
    result.slot_or_timestamp = slot;
    std::move(callback).Run(result);
    return;
  }
  GetState(rom_data->crc, slot, std::move(callback));
}

void NESRuntime::GetAutoSavedStatesCount(
    kiwi::base::OnceCallback<void(int)> callback) {
  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!rom_data) {
    std::move(callback).Run(0);
    return;
  }
  GetAutoSavedStatesCount(rom_data->crc, std::move(callback));
}

void NESRuntime::GetAutoSavedState(int slot, StateResultCallback callback) {
  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!rom_data) {
    StateResult result;
    std::move(callback).Run(result);
    return;
  }
  GetAutoSavedState(rom_data->crc, slot, std::move(callback));
}

void NESRuntime::GetAutoSavedStateByTimestamp(uint64_t timestamp,
                                              StateResultCallback callback) {
  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!rom_data) {
    StateResult result;
    result.slot_or_timestamp = static_cast<int>(timestamp);
    std::move(callback).Run(result);
    return;
  }
  GetAutoSavedStateByTimestamp(rom_data->crc, timestamp, std::move(callback));
}

void NESRuntime::DeleteAutoSavedStatesNewerThan(
    uint64_t timestamp,
    kiwi::base::OnceCallback<void(bool)> callback) {
  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!rom_data) {
    std::move(callback).Run(false);
    return;
  }
  DeleteAutoSavedStatesNewerThan(rom_data->crc, timestamp, std::move(callback));
}

void NESRuntime::SaveState(
    int crc32,
    int slot,
    const kiwi::nes::Bytes& saved_state,
    const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail,
    kiwi::base::OnceCallback<void(bool)> callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&SaveStateOnIOThread, profile_path(), crc32, slot,
                           saved_state, thumbnail),
      std::move(callback));
}

void NESRuntime::LoadROM(const kiwi::base::FilePath& rom_path,
                         kiwi::emu::Emulator::LoadCallback callback) {
  LoadROM(rom_path, std::move(callback), std::nullopt);
}

void NESRuntime::LoadROM(const kiwi::base::FilePath& rom_path,
                         kiwi::nes::Emulator::LoadCallback callback,
                         const std::optional<kiwi::base::FilePath>& save_path) {
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, kiwi::base::BindOnce(&kiwi::base::ReadFileToBytes, rom_path),
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, NESRuntime* runtime_data,
             std::optional<kiwi::base::FilePath> save_path,
             kiwi::nes::Emulator::LoadCallback callback,
             std::optional<std::vector<uint8_t>> rom_data) {
            if (weak_lifetime.expired()) {
              return;
            }
            if (!rom_data) {
              std::move(callback).Run(false);
              return;
            }
            runtime_data->LoadROMData(std::move(*rom_data), std::move(callback),
                                      kiwi::nes::Emulator::LoadOptions{},
                                      save_path);
          },
          std::move(weak_lifetime), this, save_path, std::move(callback)));
}

void NESRuntime::LoadROM(kiwi::emu::Bytes rom_data,
                         kiwi::emu::Emulator::LoadCallback callback) {
  LoadROMData(std::move(rom_data), std::move(callback),
              kiwi::nes::Emulator::LoadOptions{}, std::nullopt);
}

void NESRuntime::LoadROM(kiwi::nes::Bytes rom_data,
                         kiwi::nes::Emulator::LoadCallback callback,
                         const kiwi::nes::Emulator::LoadOptions& options) {
  LoadROMData(std::move(rom_data), std::move(callback), options, std::nullopt);
}

void NESRuntime::LoadROMData(
    kiwi::nes::Bytes rom_data,
    kiwi::nes::Emulator::LoadCallback callback,
    const kiwi::nes::Emulator::LoadOptions& options,
    const std::optional<kiwi::base::FilePath>& save_path) {
  const bool resume_old_rom = nes_emulator()->GetRunningState() ==
                              kiwi::nes::Emulator::RunningState::kRunning;
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  kiwi::nes::Emulator::LoadCallback tracked_callback = kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, NESRuntime* runtime_data,
         std::optional<kiwi::base::FilePath> save_path,
         kiwi::nes::Emulator::LoadCallback callback, bool success) {
        if (weak_lifetime.expired()) {
          return;
        }
        if (success) {
          runtime_data->current_battery_save_path_ = std::move(save_path);
        }
        std::move(callback).Run(success);
      },
      weak_lifetime, this, save_path, std::move(callback));
  PauseAndFlushCurrentROM(
      kiwi::base::BindOnce(&OnCurrentROMSavedForLoad, std::move(weak_lifetime),
                           this, std::move(rom_data), options, save_path,
                           resume_old_rom, std::move(tracked_callback)));
}

void NESRuntime::UnloadROM(kiwi::nes::Emulator::LoadCallback callback) {
  const bool resume_old_rom = nes_emulator()->GetRunningState() ==
                              kiwi::nes::Emulator::RunningState::kRunning;
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  kiwi::nes::Emulator::LoadCallback tracked_callback = kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, NESRuntime* runtime_data,
         kiwi::nes::Emulator::LoadCallback callback, bool success) {
        if (weak_lifetime.expired()) {
          return;
        }
        if (success) {
          runtime_data->current_battery_save_path_.reset();
        }
        std::move(callback).Run(success);
      },
      weak_lifetime, this, std::move(callback));
  PauseAndFlushCurrentROM(kiwi::base::BindOnce(
      &OnCurrentROMSavedForUnload, std::move(weak_lifetime), this,
      resume_old_rom, std::move(tracked_callback)));
}

void NESRuntime::PauseAndFlushCurrentROM(
    kiwi::nes::Emulator::LoadCallback callback) {
  if (nes_emulator()->GetRunningState() ==
      kiwi::nes::Emulator::RunningState::kRunning) {
    nes_emulator()->Pause();
  }
  FlushBatterySave(std::move(callback));
}

void NESRuntime::FlushBatterySave(kiwi::nes::Emulator::LoadCallback callback) {
  battery_save_flush_callbacks_.push_back(std::move(callback));
  if (battery_save_flush_in_progress_) {
    battery_save_flush_requested_ = true;
    return;
  }
  StartBatterySaveFlush();
}

void NESRuntime::StartBatterySaveFlush() {
  SDL_assert(!battery_save_flush_in_progress_);
  battery_save_flush_in_progress_ = true;

  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!rom_data || rom_data->prg_nvram_size == 0) {
    OnBatterySaveFlushed(true);
    return;
  }

  const bool needs_task_service = !emulator()->IsRunning();
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  std::weak_ptr<int> completion_lifetime = weak_lifetime;
  nes_emulator()->ExportPRGNVRAM(
      true, kiwi::base::BindOnce(&OnPRGNVRAMExported, std::move(weak_lifetime),
                                 this, current_battery_save_path_,
                                 kiwi::base::BindOnce(
                                     [](std::weak_ptr<int> weak_lifetime,
                                        NESRuntime* runtime, bool success) {
                                       if (weak_lifetime.expired()) {
                                         return;
                                       }
                                       runtime->OnBatterySaveFlushed(success);
                                     },
                                     std::move(completion_lifetime), this)));
  if (needs_task_service) {
    emulator()->RunOneFrame();
  }
}

void NESRuntime::OnBatterySaveFlushed(bool success) {
  SDL_assert(battery_save_flush_in_progress_);
  battery_save_flush_in_progress_ = false;

  if (success && battery_save_flush_requested_) {
    battery_save_flush_requested_ = false;
    StartBatterySaveFlush();
    return;
  }

  battery_save_flush_requested_ = false;
  auto callbacks = std::move(battery_save_flush_callbacks_);
  battery_save_flush_callbacks_.clear();
  for (auto& callback : callbacks) {
    std::move(callback).Run(success);
  }
}

void NESRuntime::GetSavedStatesCount(
    int crc32,
    kiwi::base::OnceCallback<void(int)> callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetSavedStatesCountOnIOThread, profile_path(),
                           crc32),
      std::move(callback));
}

void NESRuntime::GetAutoSavedStatesCount(
    int crc32,
    kiwi::base::OnceCallback<void(int)> callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStatesCountOnIOThread, profile_path(),
                           crc32),
      std::move(callback));
}

void NESRuntime::GetAutoSavedState(
    int crc32,
    int slot,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStateOnIOThread, profile_path(), crc32,
                           slot),
      std::move(load_callback));
}

void NESRuntime::GetAutoSavedStateByTimestamp(
    int crc32,
    uint64_t timestamp,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStateByTimestampOnIOThread,
                           profile_path(), crc32, timestamp),
      std::move(load_callback));
}

void NESRuntime::DeleteAutoSavedStatesNewerThan(
    int crc32,
    uint64_t timestamp,
    kiwi::base::OnceCallback<void(bool)> callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&DeleteAutoSavedStatesNewerThanOnIOThread,
                           profile_path(), crc32, timestamp),
      std::move(callback));
}

void NESRuntime::GetState(
    int crc32,
    int slot,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetStateOnIOThread, profile_path(), crc32, slot),
      std::move(load_callback));
}

void NESRuntime::TriggerDelayedAutoSave(kiwi::base::TimeDelta delta,
                                        GetThumbnailCallback thumbnail,
                                        uint64_t timer_generation) {
  if (!auto_save_started_ || timer_generation != auto_save_timer_generation_) {
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetTimerTaskRunner()->PostDelayedTask(
      FROM_HERE,
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, NESRuntime* runtime,
             kiwi::base::TimeDelta delta, GetThumbnailCallback thumbnail,
             uint64_t timer_generation) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->RunAutoStateSave(delta, std::move(thumbnail),
                                      timer_generation);
          },
          std::move(weak_lifetime), this, delta, std::move(thumbnail),
          timer_generation),
      delta);
}

void NESRuntime::RunAutoStateSave(kiwi::base::TimeDelta delta,
                                  GetThumbnailCallback thumbnail,
                                  uint64_t timer_generation) {
  if (!auto_save_started_ || timer_generation != auto_save_timer_generation_) {
    return;
  }

  const kiwi::nes::RomData* rom_data = nes_emulator()->GetRomData();
  if (!emulator()->IsRunning() || !rom_data) {
    TriggerDelayedAutoSave(delta, std::move(thumbnail), timer_generation);
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  nes_emulator()->SaveState(kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, NESRuntime* runtime,
         kiwi::base::TimeDelta delta, GetThumbnailCallback thumbnail,
         uint64_t timer_generation, int crc32, kiwi::nes::Bytes data) {
        if (weak_lifetime.expired()) {
          return;
        }
        runtime->OnAutoStateCaptured(delta, std::move(thumbnail),
                                     timer_generation, crc32, std::move(data));
      },
      std::move(weak_lifetime), this, delta, std::move(thumbnail),
      timer_generation, rom_data->crc));
}

void NESRuntime::OnAutoStateCaptured(kiwi::base::TimeDelta delta,
                                     GetThumbnailCallback thumbnail,
                                     uint64_t timer_generation,
                                     int crc32,
                                     kiwi::nes::Bytes data) {
  if (!auto_save_started_ || timer_generation != auto_save_timer_generation_) {
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&SaveAutoSavedStateOnIOThread,
                           std::chrono::system_clock::to_time_t(
                               std::chrono::system_clock::now()),
                           profile_path(), crc32, std::move(data),
                           thumbnail.Run()),
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, NESRuntime* runtime,
             kiwi::base::TimeDelta delta, GetThumbnailCallback thumbnail,
             uint64_t timer_generation, bool success) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->OnAutoStateWritten(delta, std::move(thumbnail),
                                        timer_generation, success);
          },
          std::move(weak_lifetime), this, delta, std::move(thumbnail),
          timer_generation));
}

void NESRuntime::OnAutoStateWritten(kiwi::base::TimeDelta delta,
                                    GetThumbnailCallback thumbnail,
                                    uint64_t timer_generation,
                                    bool success) {
  if (!success) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Can't auto save state.");
  }
  TriggerDelayedAutoSave(delta, std::move(thumbnail), timer_generation);
}

void NESRuntime::StartAutoStateSave(kiwi::base::TimeDelta delta,
                                    GetThumbnailCallback thumbnail) {
  if (auto_save_started_) {
    return;
  }

  auto_save_started_ = true;
  TriggerDelayedAutoSave(delta, std::move(thumbnail),
                         ++auto_save_timer_generation_);
}

void NESRuntime::StopAutoStateSave() {
  auto_save_started_ = false;
  ++auto_save_timer_generation_;
}

void NESRuntime::OnStopPeriodicSaves() {
  StopAutoStateSave();
}

#if BUILDFLAG(IS_WASM)

bool NESRuntime::SaveStateExists(int crc32, int slot) {
  kiwi::base::FilePath path_to_data =
      GetSnapshotDataPath(profile_path(), crc32, slot);
  return kiwi::base::PathExists(path_to_data);
}

kiwi::nes::Bytes NESRuntime::ReadSaveStateThumbnail(int crc32, int slot) {
  kiwi::base::FilePath path_to_thumbnail =
      GetSnapshotThumbnailPath(profile_path(), crc32, slot);
  if (!kiwi::base::PathExists(path_to_thumbnail)) {
    return {};
  }
  auto result = kiwi::base::ReadFileToBytes(path_to_thumbnail);
  if (result.has_value()) {
    return result.value();
  }
  return {};
}

bool NESRuntime::DeleteSaveState(int crc32, int slot) {
  kiwi::base::FilePath path_to_snapshot =
      GetSnapshotPath(profile_path(), crc32, slot);
  return kiwi::base::DeletePathRecursively(path_to_snapshot);
}

#endif
