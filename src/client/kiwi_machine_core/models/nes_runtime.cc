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
#include <set>
#include <vector>

#include "ui/application.h"
#include "ui/widgets/canvas.h"

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
  char* pref_path = SDL_GetPrefPath("Kiwi", "KiwiMachine");
  kiwi::base::FilePath profile_path =
      kiwi::base::FilePath::FromUTF8Unsafe(pref_path).Append(name);
  SDL_free(pref_path);
  return profile_path;
}

kiwi::base::FilePath GetStatesPath(const kiwi::base::FilePath& profile_path,
                                   int crc32) {
  kiwi::base::FilePath path_to_states =
      profile_path.Append(kiwi::base::FilePath::FromUTF8Unsafe("States"));
  path_to_states = path_to_states.Append(kiwi::base::NumberToString(crc32));
  return path_to_states;
}

kiwi::base::FilePath GetAutoSavedStatePath(
    const kiwi::base::FilePath& profile_path,
    int crc32) {
  kiwi::base::FilePath auto_saved_snapshot_path =
      GetStatesPath(profile_path, crc32);
  auto_saved_snapshot_path = auto_saved_snapshot_path.Append("AutoSaved");
  return auto_saved_snapshot_path;
}

// Layout of a NESRuntime snapshot:
// ./States/{CRC}/{Slot}/data
// ./States/{CRC}/{Slot}/thumbnail
kiwi::base::FilePath GetSnapshotPath(const kiwi::base::FilePath& profile_path,
                                     int crc32,
                                     int slot) {
  kiwi::base::FilePath path_to_snapshot = GetStatesPath(profile_path, crc32);
  path_to_snapshot = path_to_snapshot.Append(kiwi::base::NumberToString(slot));
  return path_to_snapshot;
}

kiwi::base::FilePath GetSnapshotDataPath(const kiwi::base::FilePath& path) {
  return path.Append("data");
}

kiwi::base::FilePath GetSnapshotThumbnailPath(
    const kiwi::base::FilePath& path) {
  return path.Append("thumbnail");
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
  if (!std::filesystem::exists(path_to_data.DirName()))
    std::filesystem::create_directories(path_to_data.DirName());

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
      bool deleted = std::filesystem::remove_all(*files.rbegin());
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
  if (!std::filesystem::exists(auto_saved_snapshot_path))
    std::filesystem::create_directories(auto_saved_snapshot_path);

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
  kiwi::base::File file(path, kiwi::base::File::FLAG_READ);
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
  if (!std::filesystem::exists(auto_saved_snapshot_path)) {
    bool created =
        std::filesystem::create_directories(auto_saved_snapshot_path);
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

}  // namespace

NESRuntime::NESRuntime() {
  task_runner_ = kiwi::base::SequencedTaskRunner::GetCurrentDefault();
  SDL_assert(task_runner_);
}

NESRuntime::~NESRuntime() = default;

void NESRuntime::Data::SaveState(
    int crc32,
    int slot,
    const kiwi::nes::Bytes& saved_state,
    const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail,
    kiwi::base::OnceCallback<void(bool)> callback) {
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&SaveStateOnIOThread, profile_path, crc32, slot,
                           saved_state, thumbnail),
      std::move(callback));
}

void SaveAutoSavedState(
    const kiwi::base::FilePath& profile_path,
    int crc32,
    const kiwi::nes::Bytes& saved_state,
    const kiwi::nes::IODevices::RenderDevice::Buffer& thumbnail,
    kiwi::base::OnceCallback<void(bool)> callback) {
  time_t now =
      std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&SaveAutoSavedStateOnIOThread, now, profile_path,
                           crc32, saved_state, thumbnail),
      std::move(callback));
}

void NESRuntime::Data::GetAutoSavedStatesCount(
    int crc32,
    kiwi::base::OnceCallback<void(int)> callback) {
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStatesCountOnIOThread, profile_path,
                           crc32),
      std::move(callback));
}

void NESRuntime::Data::GetAutoSavedState(
    int crc32,
    int slot,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStateOnIOThread, profile_path, crc32,
                           slot),
      std::move(load_callback));
}

void NESRuntime::Data::GetAutoSavedStateByTimestamp(
    int crc32,
    uint64_t timestamp,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStateByTimestampOnIOThread,
                           profile_path, crc32, timestamp),
      std::move(load_callback));
}

void NESRuntime::Data::GetState(
    int crc32,
    int slot,
    kiwi::base::OnceCallback<void(const StateResult&)> load_callback) {
  Application::Get()->GetIOTaskRunner()->PostTaskAndReplyWithResult(
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
                // This method will be called on NESRuntime's thread.
                SDL_assert(NESRuntime::GetInstance()
                               ->task_runner()
                               ->RunsTasksInCurrentSequence());

                SaveAutoSavedState(
                    runtime_data->profile_path, crc, data, thumbnail.Run(),
                    kiwi::base::BindOnce(
                        [](NESRuntime::Data* runtime_data,
                           kiwi::base::TimeDelta delta,
                           GetThumbnailCallback thumbnail, bool success) {
                          // This method will be called on NESRuntime's
                          // thread.
                          SDL_assert(NESRuntime::GetInstance()
                                         ->task_runner()
                                         ->RunsTasksInCurrentSequence());

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
    NESRuntime::GetInstance()->task_runner()->PostDelayedTask(
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
  if (!std::filesystem::exists(profile_path)) {
    bool created = std::filesystem::create_directory(profile_path);
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