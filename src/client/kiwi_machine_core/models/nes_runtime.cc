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

#include "models/nes_runtime.h"

#include <SDL.h>
#include <SDL_image.h>
#include <time.h>
#include <tiny_jpeg.h>
#include <chrono>
#include <memory>
#include <set>
#include <vector>

#include "models/battery_save.h"
#include "nes/rom_hash.h"
#include "ui/application.h"
#include "ui/widgets/canvas.h"

#if KIWI_WASM
#include "utility/emscripten/bridge_api.h"
#endif

namespace {
std::vector<std::unique_ptr<NESRuntime::Data>> g_runtime_data;

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

kiwi::base::FilePath GetProfilePath(const std::string& name) {
#if KIWI_WASM
  // In WASM, use /persistent directory for IDBFS
  return kiwi::base::FilePath::FromUTF8Unsafe("/persistent")
      .Append(kiwi::base::FilePath::FromUTF8Unsafe(name));
#else
  char* pref_path = SDL_GetPrefPath("Kiwi", "KiwiMachine");
  kiwi::base::FilePath profile_path =
      kiwi::base::FilePath::FromUTF8Unsafe(pref_path).Append(
          kiwi::base::FilePath::FromUTF8Unsafe(name));
  SDL_free(pref_path);
  return profile_path;
#endif
}

kiwi::base::FilePath GetStatesPath(const kiwi::base::FilePath& profile_path,
                                   int crc32) {
  kiwi::base::FilePath path_to_states =
      profile_path.Append(FILE_PATH_LITERAL("States"));
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
// ./States/{CRC}/{Slot}/data
// ./States/{CRC}/{Slot}/thumbnail
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

#if KIWI_WASM
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

    int max_auto_save_states = NESRuntime::Data::MaxAutoSaveStates;
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

NESRuntime::Data::StateResult GetStateByPathOnIOThread(
    const kiwi::base::FilePath& path_to_data,
    const kiwi::base::FilePath& path_to_thumbnail) {
  NESRuntime::Data::StateResult sr{false};
  sr.state_data = ReadDataFromProfile(path_to_data.AsUTF8Unsafe());

  // Thumbnail is JPEG format, convert it to pixel data:
  kiwi::nes::Bytes source =
      ReadDataFromProfile(path_to_thumbnail.AsUTF8Unsafe());
  if (!source.empty()) {
    SDL_RWops* rw = SDL_RWFromConstMem(source.data(), source.size());
    SDL_Surface* surface = IMG_Load_RW(rw, true);
    if (surface->format->format == SDL_PIXELFORMAT_RGB24) {
      sr.thumbnail_data.resize(4 * surface->w * surface->h);
      kiwi::nes::Byte* ptr = reinterpret_cast<unsigned char*>(surface->pixels);
      kiwi::nes::Byte* end = reinterpret_cast<unsigned char*>(surface->pixels) +
                             (surface->pitch * surface->h);
      size_t index = 0;
      while (ptr < end) {
        sr.thumbnail_data.at(index++) = ptr[0];
        sr.thumbnail_data.at(index++) = ptr[1];
        sr.thumbnail_data.at(index++) = ptr[2];
        sr.thumbnail_data.at(index++) = 0xff;
        ptr += 3;
      }

    } else if (surface->format->format == SDL_PIXELFORMAT_RGBA8888) {
      sr.thumbnail_data.resize(surface->pitch * surface->h);
      memcpy(sr.thumbnail_data.data(), surface->pixels,
             sr.thumbnail_data.size());
    } else {
      SDL_assert(false);  // Format doesn't support.
    }
    SDL_FreeSurface(surface);
  }

  sr.success = !sr.state_data.empty() && !sr.thumbnail_data.empty();
  return sr;
}

NESRuntime::Data::StateResult GetStateOnIOThread(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    int slot) {
  kiwi::base::FilePath path_to_data =
      GetSnapshotDataPath(profile_path, crc32, slot);
  kiwi::base::FilePath path_to_thumbnail =
      GetSnapshotThumbnailPath(profile_path, crc32, slot);
  NESRuntime::Data::StateResult sr =
      GetStateByPathOnIOThread(path_to_data, path_to_thumbnail);
  sr.slot_or_timestamp = slot;
  return sr;
}

NESRuntime::Data::StateResult GetAutoSavedStateByTimestampOnIOThread(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    uint64_t timestamp) {
  kiwi::base::FilePath auto_saved_snapshot_path =
      GetAutoSavedStatePath(profile_path, crc32);
  kiwi::base::FilePath state_path = auto_saved_snapshot_path.Append(
      kiwi::base::FilePath::FromUTF8Unsafe(std::to_string(timestamp)));

  NESRuntime::Data::StateResult sr = GetStateByPathOnIOThread(
      GetSnapshotDataPath(state_path), GetSnapshotThumbnailPath(state_path));
  sr.slot_or_timestamp = timestamp;
  return sr;
}

NESRuntime::Data::StateResult GetAutoSavedStateOnIOThread(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    int slot) {
  NESRuntime::Data::StateResult sr{false};
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

void OnBatterySaveWritten(NESRuntime::Data* runtime_data,
                          std::string rom_sha1,
                          uint64_t generation,
                          kiwi::nes::Emulator::LoadCallback callback,
                          bool success) {
  if (!success) {
    std::move(callback).Run(false);
    return;
  }
  runtime_data->emulator->AcknowledgePRGNVRAMSaved(rom_sha1, generation,
                                                   std::move(callback));
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
    NESRuntime::Data* runtime_data,
    std::optional<kiwi::base::FilePath> explicit_save_path,
    kiwi::nes::Emulator::LoadCallback callback,
    std::optional<kiwi::nes::Emulator::PRGNVRAMSnapshot> snapshot) {
  if (!snapshot) {
    std::move(callback).Run(true);
    return;
  }

  const std::string rom_sha1 = snapshot->rom_sha1;
  const uint64_t generation = snapshot->generation;
  runtime_data->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&WriteBatterySave, runtime_data->profile_path,
                           std::move(explicit_save_path), rom_sha1,
                           std::move(snapshot->data)),
      kiwi::base::BindOnce(&OnBatterySaveWritten, runtime_data, rom_sha1,
                           generation, std::move(callback)));
}

void ResumeOldROMAfterFailure(NESRuntime::Data* runtime_data,
                              bool resume_old_rom) {
  if (resume_old_rom) {
    runtime_data->emulator->Run();
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

void OnPreparedROMLoaded(NESRuntime::Data* runtime_data,
                         std::shared_ptr<const kiwi::nes::Bytes> rom_data,
                         kiwi::nes::Emulator::LoadOptions options,
                         bool retry_without_prg_nvram,
                         std::optional<kiwi::base::FilePath> invalid_save_path,
                         bool resume_old_rom,
                         kiwi::nes::Emulator::LoadCallback callback,
                         bool success);

void OnInvalidBatterySaveBackedUp(
    NESRuntime::Data* runtime_data,
    std::shared_ptr<const kiwi::nes::Bytes> rom_data,
    kiwi::nes::Emulator::LoadOptions options,
    bool resume_old_rom,
    kiwi::nes::Emulator::LoadCallback callback,
    std::optional<kiwi::base::FilePath> backup_path) {
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
  runtime_data->emulator->LoadAndRun(
      *rom_data,
      kiwi::base::BindOnce(&OnPreparedROMLoaded, runtime_data, rom_data,
                           options, false,
                           std::optional<kiwi::base::FilePath>(),
                           resume_old_rom, std::move(callback)),
      options);
}

void OnPreparedROMLoaded(NESRuntime::Data* runtime_data,
                         std::shared_ptr<const kiwi::nes::Bytes> rom_data,
                         kiwi::nes::Emulator::LoadOptions options,
                         bool retry_without_prg_nvram,
                         std::optional<kiwi::base::FilePath> invalid_save_path,
                         bool resume_old_rom,
                         kiwi::nes::Emulator::LoadCallback callback,
                         bool success) {
  if (success) {
    std::move(callback).Run(true);
    return;
  }
  if (retry_without_prg_nvram && invalid_save_path) {
    runtime_data->GetIOTaskRunner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        kiwi::base::BindOnce(&battery_save::BackupInvalid, *invalid_save_path),
        kiwi::base::BindOnce(&OnInvalidBatterySaveBackedUp, runtime_data,
                             rom_data, options, resume_old_rom,
                             std::move(callback)));
    return;
  }

  ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
  std::move(callback).Run(false);
}

void LoadPreparedROM(NESRuntime::Data* runtime_data,
                     bool resume_old_rom,
                     kiwi::nes::Emulator::LoadCallback callback,
                     std::optional<PreparedROM> prepared) {
  if (!prepared) {
    ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
    std::move(callback).Run(false);
    return;
  }

  auto rom_data =
      std::make_shared<const kiwi::nes::Bytes>(std::move(prepared->data));
  kiwi::nes::Emulator::LoadOptions options = std::move(prepared->options);
  kiwi::nes::Emulator::LoadCallback load_callback = kiwi::base::BindOnce(
      &OnPreparedROMLoaded, runtime_data, rom_data, options,
      prepared->retry_without_prg_nvram, std::move(prepared->invalid_save_path),
      resume_old_rom, std::move(callback));
  if (prepared->initial_prg_nvram) {
    runtime_data->emulator->LoadAndRunWithPRGNVRAM(
        *rom_data, *prepared->initial_prg_nvram, std::move(load_callback),
        options);
  } else {
    runtime_data->emulator->LoadAndRun(*rom_data, std::move(load_callback),
                                       options);
  }
}

void OnCurrentROMSavedForLoad(NESRuntime::Data* runtime_data,
                              kiwi::nes::Bytes rom_data,
                              kiwi::nes::Emulator::LoadOptions options,
                              std::optional<kiwi::base::FilePath> save_path,
                              bool resume_old_rom,
                              kiwi::nes::Emulator::LoadCallback callback,
                              bool success) {
  if (!success) {
    ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
    std::move(callback).Run(false);
    return;
  }

  runtime_data->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&PrepareROMData, runtime_data->profile_path,
                           std::move(rom_data), std::move(options),
                           std::move(save_path)),
      kiwi::base::BindOnce(&LoadPreparedROM, runtime_data, resume_old_rom,
                           std::move(callback)));
}

void OnCurrentROMSavedForUnload(NESRuntime::Data* runtime_data,
                                bool resume_old_rom,
                                kiwi::nes::Emulator::LoadCallback callback,
                                bool success) {
  if (!success) {
    ResumeOldROMAfterFailure(runtime_data, resume_old_rom);
    std::move(callback).Run(false);
    return;
  }
  runtime_data->emulator->Unload(kiwi::base::BindOnce(
      [](kiwi::nes::Emulator::LoadCallback callback) {
        std::move(callback).Run(true);
      },
      std::move(callback)));
}

}  // namespace

NESRuntime::NESRuntime() = default;
NESRuntime::~NESRuntime() = default;

NESRuntime::Data::Data() = default;

NESRuntime::Data::Data(
    scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<kiwi::base::SequencedTaskRunner> timer_task_runner)
    : io_task_runner_(std::move(io_task_runner)),
      timer_task_runner_(std::move(timer_task_runner)) {}

scoped_refptr<kiwi::base::SequencedTaskRunner>
NESRuntime::Data::GetIOTaskRunner() {
  return io_task_runner_ ? io_task_runner_
                         : Application::Get()->GetIOTaskRunner();
}

void NESRuntime::Data::SaveState(
    int crc32,
    int slot,
    const kiwi::nes::Bytes& saved_state,
    const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail,
    kiwi::base::OnceCallback<void(bool)> callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&SaveStateOnIOThread, profile_path, crc32, slot,
                           saved_state, thumbnail),
      std::move(callback));
}

void NESRuntime::Data::LoadROM(
    const kiwi::base::FilePath& rom_path,
    kiwi::nes::Emulator::LoadCallback callback,
    const std::optional<kiwi::base::FilePath>& save_path) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE, kiwi::base::BindOnce(&kiwi::base::ReadFileToBytes, rom_path),
      kiwi::base::BindOnce(
          [](NESRuntime::Data* runtime_data,
             std::optional<kiwi::base::FilePath> save_path,
             kiwi::nes::Emulator::LoadCallback callback,
             std::optional<std::vector<uint8_t>> rom_data) {
            if (!rom_data) {
              std::move(callback).Run(false);
              return;
            }
            runtime_data->LoadROMData(std::move(*rom_data), std::move(callback),
                                      kiwi::nes::Emulator::LoadOptions{},
                                      save_path);
          },
          this, save_path, std::move(callback)));
}

void NESRuntime::Data::LoadROM(
    kiwi::nes::Bytes rom_data,
    kiwi::nes::Emulator::LoadCallback callback,
    const kiwi::nes::Emulator::LoadOptions& options) {
  LoadROMData(std::move(rom_data), std::move(callback), options, std::nullopt);
}

void NESRuntime::Data::LoadROMData(
    kiwi::nes::Bytes rom_data,
    kiwi::nes::Emulator::LoadCallback callback,
    const kiwi::nes::Emulator::LoadOptions& options,
    const std::optional<kiwi::base::FilePath>& save_path) {
  const bool resume_old_rom = emulator->GetRunningState() ==
                              kiwi::nes::Emulator::RunningState::kRunning;
  kiwi::nes::Emulator::LoadCallback tracked_callback = kiwi::base::BindOnce(
      [](NESRuntime::Data* runtime_data,
         std::optional<kiwi::base::FilePath> save_path,
         kiwi::nes::Emulator::LoadCallback callback, bool success) {
        if (success) {
          runtime_data->current_battery_save_path_ = std::move(save_path);
        }
        std::move(callback).Run(success);
      },
      this, save_path, std::move(callback));
  PauseAndFlushCurrentROM(kiwi::base::BindOnce(
      &OnCurrentROMSavedForLoad, this, std::move(rom_data), options, save_path,
      resume_old_rom, std::move(tracked_callback)));
}

void NESRuntime::Data::UnloadROM(kiwi::nes::Emulator::LoadCallback callback) {
  const bool resume_old_rom = emulator->GetRunningState() ==
                              kiwi::nes::Emulator::RunningState::kRunning;
  kiwi::nes::Emulator::LoadCallback tracked_callback = kiwi::base::BindOnce(
      [](NESRuntime::Data* runtime_data,
         kiwi::nes::Emulator::LoadCallback callback, bool success) {
        if (success) {
          runtime_data->current_battery_save_path_.reset();
        }
        std::move(callback).Run(success);
      },
      this, std::move(callback));
  PauseAndFlushCurrentROM(kiwi::base::BindOnce(&OnCurrentROMSavedForUnload,
                                               this, resume_old_rom,
                                               std::move(tracked_callback)));
}

void NESRuntime::Data::PauseAndFlushCurrentROM(
    kiwi::nes::Emulator::LoadCallback callback) {
  if (emulator->GetRunningState() ==
      kiwi::nes::Emulator::RunningState::kRunning) {
    emulator->Pause();
  }
  FlushBatterySave(std::move(callback));
}

void NESRuntime::Data::FlushBatterySave(
    kiwi::nes::Emulator::LoadCallback callback) {
  battery_save_flush_callbacks_.push_back(std::move(callback));
  if (battery_save_flush_in_progress_) {
    battery_save_flush_requested_ = true;
    return;
  }
  StartBatterySaveFlush();
}

void NESRuntime::Data::StartBatterySaveFlush() {
  SDL_assert(!battery_save_flush_in_progress_);
  battery_save_flush_in_progress_ = true;

  const kiwi::nes::RomData* rom_data = emulator->GetRomData();
  if (!rom_data || rom_data->prg_nvram_size == 0) {
    OnBatterySaveFlushed(true);
    return;
  }

  emulator->ExportPRGNVRAM(
      true, kiwi::base::BindOnce(
                &OnPRGNVRAMExported, this, current_battery_save_path_,
                kiwi::base::BindOnce(&NESRuntime::Data::OnBatterySaveFlushed,
                                     kiwi::base::Unretained(this))));
}

void NESRuntime::Data::OnBatterySaveFlushed(bool success) {
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

void NESRuntime::Data::GetAutoSavedStatesCount(
    int crc32,
    kiwi::base::OnceCallback<void(int)> callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStatesCountOnIOThread, profile_path,
                           crc32),
      std::move(callback));
}

void NESRuntime::Data::GetAutoSavedState(
    int crc32,
    int slot,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStateOnIOThread, profile_path, crc32,
                           slot),
      std::move(load_callback));
}

void NESRuntime::Data::GetAutoSavedStateByTimestamp(
    int crc32,
    uint64_t timestamp,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStateByTimestampOnIOThread,
                           profile_path, crc32, timestamp),
      std::move(load_callback));
}

void NESRuntime::Data::GetState(
    int crc32,
    int slot,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetStateOnIOThread, profile_path, crc32, slot),
      std::move(load_callback));
}

kiwi::base::RepeatingClosure NESRuntime::Data::CreateAutoSaveClosure(
    kiwi::base::TimeDelta delta,
    GetThumbnailCallback thumbnail) {
  return kiwi::base::BindRepeating(
      [](NESRuntime::Data* runtime_data, kiwi::base::TimeDelta delta,
         GetThumbnailCallback thumbnail) {
        auto* rom_data = runtime_data->emulator->GetRomData();
        if (runtime_data->emulator->GetRunningState() ==
            kiwi::nes::Emulator::RunningState::kRunning) {
          SDL_assert(rom_data);
          if (!runtime_data->auto_save_started_)
            return;

          runtime_data->emulator->SaveState(kiwi::base::BindOnce(
              [](NESRuntime::Data* runtime_data, int crc,
                 kiwi::base::TimeDelta delta, GetThumbnailCallback thumbnail,
                 kiwi::nes::Bytes data) {
                runtime_data->GetIOTaskRunner()->PostTaskAndReplyWithResult(
                    FROM_HERE,
                    kiwi::base::BindOnce(&SaveAutoSavedStateOnIOThread,
                                         std::chrono::system_clock::to_time_t(
                                             std::chrono::system_clock::now()),
                                         runtime_data->profile_path, crc, data,
                                         thumbnail.Run()),
                    kiwi::base::BindOnce(
                        [](NESRuntime::Data* runtime_data,
                           kiwi::base::TimeDelta delta,
                           GetThumbnailCallback thumbnail, bool success) {
                          if (!success) {
                            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                                        "Can't auto save state.");
                          }

                          // Invoke it again if auto start.
                          runtime_data->TriggerDelayedAutoSave(delta,
                                                               thumbnail);
                        },
                        runtime_data, delta, thumbnail));
              },
              runtime_data, rom_data->crc, delta, thumbnail));
        } else {
          runtime_data->TriggerDelayedAutoSave(delta, thumbnail);
        }
      },
      this, delta, thumbnail);
}

void NESRuntime::Data::TriggerDelayedAutoSave(kiwi::base::TimeDelta delta,
                                              GetThumbnailCallback thumbnail) {
  if (auto_save_started_) {
    kiwi::base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, CreateAutoSaveClosure(delta, thumbnail), delta);
  }
}

void NESRuntime::Data::StartAutoSave(kiwi::base::TimeDelta delta,
                                     GetThumbnailCallback thumbnail) {
  if (auto_save_started_)
    return;

  auto_save_started_ = true;
  TriggerDelayedAutoSave(delta, thumbnail);
}

void NESRuntime::Data::StopAutoSave() {
  auto_save_started_ = false;
}

void NESRuntime::Data::StartBatterySave(kiwi::base::TimeDelta delta) {
  if (battery_save_started_) {
    return;
  }

  battery_save_started_ = true;
  TriggerDelayedBatterySave(delta, ++battery_save_timer_generation_);
}

void NESRuntime::Data::StopBatterySave() {
  battery_save_started_ = false;
  ++battery_save_timer_generation_;
}

void NESRuntime::Data::TriggerDelayedBatterySave(kiwi::base::TimeDelta delta,
                                                 uint64_t timer_generation) {
  if (!battery_save_started_ ||
      timer_generation != battery_save_timer_generation_) {
    return;
  }

  scoped_refptr<kiwi::base::SequencedTaskRunner> task_runner =
      timer_task_runner_
          ? timer_task_runner_
          : kiwi::base::SingleThreadTaskRunner::GetCurrentDefault();
  task_runner->PostDelayedTask(
      FROM_HERE,
      kiwi::base::BindOnce(&NESRuntime::Data::RunPeriodicBatterySave,
                           kiwi::base::Unretained(this), delta,
                           timer_generation),
      delta);
}

void NESRuntime::Data::RunPeriodicBatterySave(kiwi::base::TimeDelta delta,
                                              uint64_t timer_generation) {
  if (!battery_save_started_ ||
      timer_generation != battery_save_timer_generation_) {
    return;
  }
  if (emulator->GetRunningState() !=
      kiwi::nes::Emulator::RunningState::kRunning) {
    TriggerDelayedBatterySave(delta, timer_generation);
    return;
  }

  FlushBatterySave(kiwi::base::BindOnce(
      &NESRuntime::Data::OnPeriodicBatterySaveFlushed,
      kiwi::base::Unretained(this), delta, timer_generation));
}

void NESRuntime::Data::OnPeriodicBatterySaveFlushed(kiwi::base::TimeDelta delta,
                                                    uint64_t timer_generation,
                                                    bool success) {
  if (!success) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Could not periodically save battery-backed data.");
  }
  TriggerDelayedBatterySave(delta, timer_generation);
}

#if KIWI_WASM

bool NESRuntime::Data::SaveStateExists(int crc32, int slot) {
  kiwi::base::FilePath path_to_data =
      GetSnapshotDataPath(profile_path, crc32, slot);
  return kiwi::base::PathExists(path_to_data);
}

kiwi::nes::Bytes NESRuntime::Data::ReadSaveStateThumbnail(int crc32, int slot) {
  kiwi::base::FilePath path_to_thumbnail =
      GetSnapshotThumbnailPath(profile_path, crc32, slot);
  if (!kiwi::base::PathExists(path_to_thumbnail)) {
    return {};
  }
  auto result = kiwi::base::ReadFileToBytes(path_to_thumbnail);
  if (result.has_value()) {
    return result.value();
  }
  return {};
}

bool NESRuntime::Data::DeleteSaveState(int crc32, int slot) {
  kiwi::base::FilePath path_to_snapshot =
      GetSnapshotPath(profile_path, crc32, slot);
  return kiwi::base::DeletePathRecursively(path_to_snapshot);
}

#endif

NESRuntime::Data* NESRuntime::GetDataById(NESRuntimeID id) {
  return g_runtime_data[id].get();
}

NESRuntimeID NESRuntime::CreateData(const std::string& name) {
  std::unique_ptr<NESRuntime::Data> data = std::make_unique<NESRuntime::Data>();
  kiwi::base::FilePath profile_path = GetProfilePath(name);
  CreateProfileIfNotExist(data.get(), profile_path);
  data->profile_path = profile_path;
  g_runtime_data.push_back(std::move(data));
  return g_runtime_data.size() - 1;
}

void NESRuntime::CreateProfileIfNotExist(
    Data* data,
    const kiwi::base::FilePath& profile_path) {
  if (!kiwi::base::PathExists(profile_path)) {
    bool created = kiwi::base::CreateDirectory(profile_path);
    if (!created) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Can't open or create profile file at %s",
                   profile_path.AsUTF8Unsafe().c_str());
      return;
    }
  }
}

NESRuntime* NESRuntime::GetInstance() {
  static NESRuntime g_instance;
  return &g_instance;
}
