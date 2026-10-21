// Copyright (C) 2026 Yisi Yu
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

#include "models/gba/runtime.h"

#include <SDL.h>
#include <SDL_image.h>
#include <tiny_jpeg.h>
#include <zlib.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "models/battery_save.h"
#include "ui/application.h"

namespace {

uint32_t CalculateCRC32(const kiwi::gba::Bytes& data) {
  uLong checksum = crc32(0L, Z_NULL, 0);
  size_t offset = 0;
  while (offset < data.size()) {
    const size_t remaining = data.size() - offset;
    const uInt chunk = static_cast<uInt>(std::min(
        remaining, static_cast<size_t>(std::numeric_limits<uInt>::max())));
    checksum = crc32(checksum, data.data() + offset, chunk);
    offset += chunk;
  }
  return static_cast<uint32_t>(checksum);
}

kiwi::base::FilePath GetSavePath(const kiwi::base::FilePath& profile_path,
                                 uint32_t crc32) {
  return profile_path.Append(FILE_PATH_LITERAL("gba"))
      .Append(FILE_PATH_LITERAL("saves"))
      .Append(kiwi::base::FilePath::FromUTF8Unsafe(
          kiwi::base::StringPrintf("%08x.sav", crc32)));
}

kiwi::base::FilePath GetStatesPath(const kiwi::base::FilePath& profile_path,
                                   uint32_t crc32) {
  return profile_path.Append(FILE_PATH_LITERAL("gba"))
      .Append(FILE_PATH_LITERAL("States"))
      .Append(kiwi::base::FilePath::FromUTF8Unsafe(
          kiwi::base::StringPrintf("%08x", crc32)));
}

kiwi::base::FilePath GetAutoSavedStatePath(
    const kiwi::base::FilePath& profile_path,
    uint32_t crc32) {
  return GetStatesPath(profile_path, crc32)
      .Append(FILE_PATH_LITERAL("AutoSaved"));
}

kiwi::base::FilePath GetSnapshotPath(const kiwi::base::FilePath& profile_path,
                                     uint32_t crc32,
                                     int slot) {
  return GetStatesPath(profile_path, crc32)
      .Append(kiwi::base::FilePath::FromUTF8Unsafe(
          kiwi::base::StringPrintf("%d", slot)));
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
    uint32_t crc32,
    int slot) {
  return GetSnapshotDataPath(GetSnapshotPath(profile_path, crc32, slot));
}

kiwi::base::FilePath GetSnapshotThumbnailPath(
    const kiwi::base::FilePath& profile_path,
    uint32_t crc32,
    int slot) {
  return GetSnapshotThumbnailPath(GetSnapshotPath(profile_path, crc32, slot));
}

bool SaveStateByPathOnIOThread(const kiwi::base::FilePath& snapshot_path,
                               const kiwi::emu::Bytes& state_data,
                               const kiwi::emu::Colors& thumbnail) {
  if (state_data.empty() ||
      thumbnail.size() != static_cast<size_t>(kiwi::gba::kFrameWidth *
                                              kiwi::gba::kFrameHeight)) {
    return false;
  }

  if (!kiwi::base::CreateDirectory(snapshot_path) ||
      !kiwi::base::WriteFile(GetSnapshotDataPath(snapshot_path), state_data)) {
    return false;
  }

  kiwi::base::File thumbnail_file(
      GetSnapshotThumbnailPath(snapshot_path),
      kiwi::base::File::FLAG_CREATE_ALWAYS | kiwi::base::File::FLAG_WRITE);
  if (!thumbnail_file.IsValid()) {
    return false;
  }

  const int encoded = tje_encode_with_func(
      [](void* context, void* data, int size) {
        static_cast<kiwi::base::File*>(context)->WriteAtCurrentPos(
            static_cast<const char*>(data), size);
      },
      &thumbnail_file, 2, kiwi::gba::kFrameWidth, kiwi::gba::kFrameHeight, 4,
      reinterpret_cast<const unsigned char*>(thumbnail.data()),
      kiwi::gba::kFrameWidth * sizeof(thumbnail[0]));
  return encoded != 0 && thumbnail_file.GetLength() > 0;
}

bool SaveStateOnIOThread(const kiwi::base::FilePath& profile_path,
                         uint32_t crc32,
                         int slot,
                         const kiwi::emu::Bytes& state_data,
                         const kiwi::emu::Colors& thumbnail) {
  return SaveStateByPathOnIOThread(GetSnapshotPath(profile_path, crc32, slot),
                                   state_data, thumbnail);
}

int GetSavedStatesCountOnIOThread(const kiwi::base::FilePath& profile_path,
                                  uint32_t crc32) {
  int count = 0;
  for (int slot = 0; slot < GBARuntime::MaxSaveStates; ++slot) {
    if (kiwi::base::PathExists(
            GetSnapshotDataPath(profile_path, crc32, slot)) &&
        kiwi::base::PathExists(
            GetSnapshotThumbnailPath(profile_path, crc32, slot))) {
      ++count;
    }
  }
  return count;
}

GBARuntime::StateResult GetStateByPathOnIOThread(
    const kiwi::base::FilePath& data_path,
    const kiwi::base::FilePath& thumbnail_path) {
  GBARuntime::StateResult result;

  std::optional<kiwi::emu::Bytes> state_data =
      kiwi::base::ReadFileToBytes(data_path);
  std::optional<kiwi::emu::Bytes> encoded_thumbnail =
      kiwi::base::ReadFileToBytes(thumbnail_path);
  if (!state_data || state_data->empty() || !encoded_thumbnail ||
      encoded_thumbnail->empty()) {
    return result;
  }

  SDL_RWops* source =
      SDL_RWFromConstMem(encoded_thumbnail->data(), encoded_thumbnail->size());
  SDL_Surface* decoded = source ? IMG_Load_RW(source, true) : nullptr;
  if (!decoded) {
    return result;
  }

  SDL_Surface* converted =
      SDL_ConvertSurfaceFormat(decoded, SDL_PIXELFORMAT_ARGB8888, 0);
  SDL_FreeSurface(decoded);
  if (!converted) {
    return result;
  }

  result.thumbnail_width = converted->w;
  result.thumbnail_height = converted->h;
  constexpr size_t kBytesPerPixel = sizeof(kiwi::emu::Color);
  const size_t row_size = static_cast<size_t>(converted->w) * kBytesPerPixel;
  result.thumbnail_data.resize(row_size * converted->h);
  for (int row = 0; row < converted->h; ++row) {
    memcpy(
        result.thumbnail_data.data() + row * row_size,
        static_cast<const uint8_t*>(converted->pixels) + row * converted->pitch,
        row_size);
  }
  SDL_FreeSurface(converted);

  result.state_data = std::move(*state_data);
  result.success = !result.thumbnail_data.empty();
  return result;
}

GBARuntime::StateResult GetStateOnIOThread(
    const kiwi::base::FilePath& profile_path,
    uint32_t crc32,
    int slot) {
  GBARuntime::StateResult result = GetStateByPathOnIOThread(
      GetSnapshotDataPath(profile_path, crc32, slot),
      GetSnapshotThumbnailPath(profile_path, crc32, slot));
  result.slot_or_timestamp = slot;
  return result;
}

struct AutoSavedStateEntry {
  uint64_t timestamp;
  kiwi::base::FilePath path;
};

std::vector<AutoSavedStateEntry> GetAutoSavedStateEntries(
    const kiwi::base::FilePath& profile_path,
    uint32_t crc32,
    bool require_complete) {
  const kiwi::base::FilePath auto_saved_path =
      GetAutoSavedStatePath(profile_path, crc32);
  if (!kiwi::base::PathExists(auto_saved_path)) {
    return {};
  }

  std::vector<AutoSavedStateEntry> entries;
  kiwi::base::FileEnumerator enumerator(
      auto_saved_path, false, kiwi::base::FileEnumerator::DIRECTORIES);
  for (kiwi::base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    uint64_t timestamp = 0;
    if (!kiwi::base::StringToUint64(path.BaseName().AsUTF8Unsafe(),
                                    &timestamp)) {
      continue;
    }
    if (require_complete &&
        (!kiwi::base::PathExists(GetSnapshotDataPath(path)) ||
         !kiwi::base::PathExists(GetSnapshotThumbnailPath(path)))) {
      continue;
    }
    entries.push_back({timestamp, std::move(path)});
  }

  std::sort(entries.begin(), entries.end(),
            [](const AutoSavedStateEntry& lhs, const AutoSavedStateEntry& rhs) {
              return lhs.timestamp > rhs.timestamp;
            });
  return entries;
}

bool SaveAutoSavedStateOnIOThread(uint64_t timestamp,
                                  const kiwi::base::FilePath& profile_path,
                                  uint32_t crc32,
                                  const kiwi::emu::Bytes& state_data,
                                  const kiwi::emu::Colors& thumbnail) {
  const kiwi::base::FilePath snapshot_path =
      GetAutoSavedStatePath(profile_path, crc32)
          .Append(kiwi::base::FilePath::FromUTF8Unsafe(kiwi::base::StringPrintf(
              "%llu", static_cast<unsigned long long>(timestamp))));
  if (!SaveStateByPathOnIOThread(snapshot_path, state_data, thumbnail)) {
    return false;
  }

  bool success = true;
  std::vector<AutoSavedStateEntry> entries =
      GetAutoSavedStateEntries(profile_path, crc32, false);
  while (entries.size() > GBARuntime::MaxAutoSaveStates) {
    success = kiwi::base::DeletePathRecursively(entries.back().path) && success;
    entries.pop_back();
  }
  return success;
}

int GetAutoSavedStatesCountOnIOThread(const kiwi::base::FilePath& profile_path,
                                      uint32_t crc32) {
  return static_cast<int>(
      GetAutoSavedStateEntries(profile_path, crc32, true).size());
}

GBARuntime::StateResult GetAutoSavedStateOnIOThread(
    const kiwi::base::FilePath& profile_path,
    uint32_t crc32,
    int slot) {
  GBARuntime::StateResult result;
  std::vector<AutoSavedStateEntry> entries =
      GetAutoSavedStateEntries(profile_path, crc32, true);
  if (slot < 0 || slot >= static_cast<int>(entries.size())) {
    return result;
  }

  const AutoSavedStateEntry& entry = entries[slot];
  result = GetStateByPathOnIOThread(GetSnapshotDataPath(entry.path),
                                    GetSnapshotThumbnailPath(entry.path));
  result.slot_or_timestamp = static_cast<int>(entry.timestamp);
  return result;
}

GBARuntime::StateResult GetAutoSavedStateByTimestampOnIOThread(
    const kiwi::base::FilePath& profile_path,
    uint32_t crc32,
    uint64_t timestamp) {
  const kiwi::base::FilePath snapshot_path =
      GetAutoSavedStatePath(profile_path, crc32)
          .Append(kiwi::base::FilePath::FromUTF8Unsafe(kiwi::base::StringPrintf(
              "%llu", static_cast<unsigned long long>(timestamp))));
  GBARuntime::StateResult result =
      GetStateByPathOnIOThread(GetSnapshotDataPath(snapshot_path),
                               GetSnapshotThumbnailPath(snapshot_path));
  result.slot_or_timestamp = static_cast<int>(timestamp);
  return result;
}

bool DeleteAutoSavedStatesNewerThanOnIOThread(
    const kiwi::base::FilePath& profile_path,
    uint32_t crc32,
    uint64_t timestamp) {
  bool success = true;
  for (const AutoSavedStateEntry& entry :
       GetAutoSavedStateEntries(profile_path, crc32, false)) {
    if (entry.timestamp > timestamp) {
      success = kiwi::base::DeletePathRecursively(entry.path) && success;
    }
  }
  return success;
}

}  // namespace

struct GBARuntime::PreparedROM {
  std::shared_ptr<const kiwi::gba::Bytes> data;
  kiwi::base::FilePath save_path;
  std::optional<kiwi::gba::Bytes> save_data;
};

GBARuntime::GBARuntime(
    const kiwi::base::FilePath& profile_path,
    scoped_refptr<kiwi::gba::Emulator> emulator,
    scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner,
    scoped_refptr<kiwi::base::SequencedTaskRunner> timer_task_runner)
    : Runtime(profile_path, std::move(timer_task_runner)),
      emulator_(emulator ? std::move(emulator) : kiwi::gba::CreateEmulator()),
      io_task_runner_(std::move(io_task_runner)) {}

GBARuntime::~GBARuntime() {
  Shutdown();
}

scoped_refptr<kiwi::emu::Emulator> GBARuntime::emulator() {
  return emulator_;
}

kiwi::gba::Emulator* GBARuntime::gba_emulator() {
  return emulator_.get();
}

const kiwi::gba::Emulator* GBARuntime::gba_emulator() const {
  return emulator_.get();
}

scoped_refptr<kiwi::base::SequencedTaskRunner> GBARuntime::GetIOTaskRunner() {
  return io_task_runner_ ? io_task_runner_
                         : Application::Get()->GetIOTaskRunner();
}

void GBARuntime::PersistState(int slot,
                              const kiwi::emu::Bytes& state_data,
                              const kiwi::emu::Colors& thumbnail,
                              kiwi::base::OnceCallback<void(bool)> callback) {
  const kiwi::gba::RomData* rom_data = gba_emulator()->GetRomData();
  if (!rom_data) {
    std::move(callback).Run(false);
    return;
  }

  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&SaveStateOnIOThread, profile_path(),
                           rom_data->crc32, slot, state_data, thumbnail),
      std::move(callback));
}

void GBARuntime::GetSavedStatesCount(
    kiwi::base::OnceCallback<void(int)> callback) {
  const kiwi::gba::RomData* rom_data = gba_emulator()->GetRomData();
  if (!rom_data) {
    std::move(callback).Run(0);
    return;
  }

  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetSavedStatesCountOnIOThread, profile_path(),
                           rom_data->crc32),
      std::move(callback));
}

void GBARuntime::GetState(int slot, StateResultCallback callback) {
  const kiwi::gba::RomData* rom_data = gba_emulator()->GetRomData();
  if (!rom_data) {
    StateResult result;
    result.slot_or_timestamp = slot;
    std::move(callback).Run(result);
    return;
  }

  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetStateOnIOThread, profile_path(), rom_data->crc32,
                           slot),
      std::move(callback));
}

void GBARuntime::GetAutoSavedStatesCount(
    kiwi::base::OnceCallback<void(int)> callback) {
  const kiwi::gba::RomData* rom_data = gba_emulator()->GetRomData();
  if (!rom_data) {
    std::move(callback).Run(0);
    return;
  }

  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStatesCountOnIOThread, profile_path(),
                           rom_data->crc32),
      std::move(callback));
}

void GBARuntime::GetAutoSavedState(int slot, StateResultCallback callback) {
  const kiwi::gba::RomData* rom_data = gba_emulator()->GetRomData();
  if (!rom_data) {
    StateResult result;
    std::move(callback).Run(result);
    return;
  }

  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStateOnIOThread, profile_path(),
                           rom_data->crc32, slot),
      std::move(callback));
}

void GBARuntime::GetAutoSavedStateByTimestamp(uint64_t timestamp,
                                              StateResultCallback callback) {
  const kiwi::gba::RomData* rom_data = gba_emulator()->GetRomData();
  if (!rom_data) {
    StateResult result;
    result.slot_or_timestamp = static_cast<int>(timestamp);
    std::move(callback).Run(result);
    return;
  }

  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GetAutoSavedStateByTimestampOnIOThread,
                           profile_path(), rom_data->crc32, timestamp),
      std::move(callback));
}

void GBARuntime::DeleteAutoSavedStatesNewerThan(
    uint64_t timestamp,
    kiwi::base::OnceCallback<void(bool)> callback) {
  const kiwi::gba::RomData* rom_data = gba_emulator()->GetRomData();
  if (!rom_data) {
    std::move(callback).Run(false);
    return;
  }

  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&DeleteAutoSavedStatesNewerThanOnIOThread,
                           profile_path(), rom_data->crc32, timestamp),
      std::move(callback));
}

void GBARuntime::LoadROM(const kiwi::base::FilePath& rom_path,
                         kiwi::emu::Emulator::LoadCallback callback) {
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GBARuntime::PrepareROMOnIOThread, profile_path(),
                           rom_path),
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
             kiwi::gba::Emulator::LoadCallback callback,
             std::optional<PreparedROM> prepared) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->OnROMPrepared(std::move(callback), std::move(prepared));
          },
          std::move(weak_lifetime), this, std::move(callback)));
}

void GBARuntime::LoadROM(kiwi::emu::Bytes rom_data,
                         kiwi::emu::Emulator::LoadCallback callback) {
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GBARuntime::PrepareROMDataOnIOThread,
                           profile_path(), std::move(rom_data)),
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
             kiwi::gba::Emulator::LoadCallback callback,
             std::optional<PreparedROM> prepared) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->OnROMPrepared(std::move(callback), std::move(prepared));
          },
          std::move(weak_lifetime), this, std::move(callback)));
}

void GBARuntime::UnloadROM(kiwi::emu::Emulator::LoadCallback callback) {
  const bool resume_old_rom = emulator()->IsRunning();
  if (resume_old_rom) {
    emulator()->Pause();
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  FlushBatterySave(kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
         bool resume_old_rom, kiwi::gba::Emulator::LoadCallback callback,
         bool success) {
        if (weak_lifetime.expired()) {
          return;
        }
        runtime->OnCurrentROMSavedForUnload(resume_old_rom, std::move(callback),
                                            success);
      },
      std::move(weak_lifetime), this, resume_old_rom, std::move(callback)));
}

void GBARuntime::FlushBatterySave(kiwi::emu::Emulator::LoadCallback callback) {
  battery_save_flush_callbacks_.push_back(std::move(callback));
  if (battery_save_flush_in_progress_) {
    battery_save_flush_requested_ = true;
    return;
  }
  StartBatterySaveFlush();
}

void GBARuntime::StartBatterySaveFlush() {
  SDL_assert(!battery_save_flush_in_progress_);
  battery_save_flush_in_progress_ = true;

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  gba_emulator()->ExportSaveData(
      true,
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
             std::optional<kiwi::gba::Emulator::SaveDataSnapshot> snapshot) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->OnSaveDataExported(std::move(snapshot));
          },
          std::move(weak_lifetime), this));
}

std::optional<GBARuntime::PreparedROM> GBARuntime::PrepareROMOnIOThread(
    const kiwi::base::FilePath& profile_path,
    const kiwi::base::FilePath& rom_path) {
  std::optional<std::vector<uint8_t>> bytes =
      kiwi::base::ReadFileToBytes(rom_path);
  if (!bytes || bytes->empty()) {
    return std::nullopt;
  }
  return PrepareROMDataOnIOThread(profile_path, std::move(*bytes));
}

std::optional<GBARuntime::PreparedROM> GBARuntime::PrepareROMDataOnIOThread(
    const kiwi::base::FilePath& profile_path,
    kiwi::gba::Bytes rom_data) {
  if (rom_data.empty())
    return std::nullopt;

  auto data = std::make_shared<const kiwi::gba::Bytes>(std::move(rom_data));
  const kiwi::base::FilePath save_path =
      GetSavePath(profile_path, CalculateCRC32(*data));
  battery_save::ReadResult save = battery_save::ReadFromPath(save_path);
  if (save.exists && !save.success) {
    if (!battery_save::BackupInvalid(save_path)) {
      return std::nullopt;
    }
    save = battery_save::ReadResult{true, false, {}};
  }

  PreparedROM prepared;
  prepared.data = std::move(data);
  prepared.save_path = save_path;
  if (save.exists) {
    prepared.save_data = std::move(save.data);
  }
  return prepared;
}

void GBARuntime::OnROMPrepared(kiwi::gba::Emulator::LoadCallback callback,
                               std::optional<PreparedROM> prepared) {
  if (!prepared) {
    std::move(callback).Run(false);
    return;
  }

  const bool resume_old_rom = emulator()->IsRunning();
  if (resume_old_rom) {
    emulator()->Pause();
  }

  PreparedROMPtr prepared_rom =
      std::make_shared<PreparedROM>(std::move(*prepared));
  PreparedROMPtr previous_rom = current_rom_;
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  FlushBatterySave(kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
         PreparedROMPtr prepared_rom, PreparedROMPtr previous_rom,
         bool resume_old_rom, kiwi::gba::Emulator::LoadCallback callback,
         bool success) {
        if (weak_lifetime.expired()) {
          return;
        }
        runtime->OnCurrentROMSavedForLoad(
            std::move(prepared_rom), std::move(previous_rom), resume_old_rom,
            std::move(callback), success);
      },
      std::move(weak_lifetime), this, std::move(prepared_rom),
      std::move(previous_rom), resume_old_rom, std::move(callback)));
}

bool GBARuntime::WriteSaveDataOnIOThread(const kiwi::base::FilePath& save_path,
                                         const kiwi::gba::Bytes& data) {
  return battery_save::WriteToPath(save_path, data);
}

void GBARuntime::OnCurrentROMSavedForLoad(
    PreparedROMPtr prepared,
    PreparedROMPtr previous_rom,
    bool resume_old_rom,
    kiwi::gba::Emulator::LoadCallback callback,
    bool success) {
  if (!success) {
    if (resume_old_rom) {
      emulator()->Run();
    }
    std::move(callback).Run(false);
    return;
  }
  previous_rom = current_rom_;
  LoadPreparedROM(std::move(prepared), std::move(previous_rom), resume_old_rom,
                  std::move(callback));
}

void GBARuntime::LoadPreparedROM(PreparedROMPtr prepared,
                                 PreparedROMPtr previous_rom,
                                 bool resume_old_rom,
                                 kiwi::gba::Emulator::LoadCallback callback) {
  const bool had_save_data = prepared->save_data.has_value();
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  kiwi::gba::Emulator::LoadCallback load_callback = kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
         PreparedROMPtr prepared, PreparedROMPtr previous_rom,
         bool resume_old_rom, bool had_save_data,
         kiwi::gba::Emulator::LoadCallback callback, bool success) {
        if (weak_lifetime.expired()) {
          return;
        }
        runtime->OnROMLoaded(std::move(prepared), std::move(previous_rom),
                             resume_old_rom, had_save_data, std::move(callback),
                             success);
      },
      std::move(weak_lifetime), this, prepared, std::move(previous_rom),
      resume_old_rom, had_save_data, std::move(callback));
  if (prepared->save_data) {
    gba_emulator()->LoadAndRunWithSaveData(
        *prepared->data, *prepared->save_data, std::move(load_callback));
  } else {
    gba_emulator()->LoadAndRun(*prepared->data, std::move(load_callback));
  }
}

void GBARuntime::OnROMLoaded(PreparedROMPtr prepared,
                             PreparedROMPtr previous_rom,
                             bool resume_old_rom,
                             bool had_save_data,
                             kiwi::gba::Emulator::LoadCallback callback,
                             bool success) {
  if (success) {
    current_rom_ = prepared;
    std::move(callback).Run(true);
    return;
  }
  if (!had_save_data) {
    RestorePreviousROM(std::move(previous_rom), resume_old_rom,
                       std::move(callback));
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&battery_save::BackupInvalid, prepared->save_path),
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
             PreparedROMPtr prepared, PreparedROMPtr previous_rom,
             bool resume_old_rom, kiwi::gba::Emulator::LoadCallback callback,
             std::optional<kiwi::base::FilePath> backup_path) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->OnInvalidSaveBackedUp(
                std::move(prepared), std::move(previous_rom), resume_old_rom,
                std::move(callback), std::move(backup_path));
          },
          std::move(weak_lifetime), this, std::move(prepared),
          std::move(previous_rom), resume_old_rom, std::move(callback)));
}

void GBARuntime::OnInvalidSaveBackedUp(
    PreparedROMPtr prepared,
    PreparedROMPtr previous_rom,
    bool resume_old_rom,
    kiwi::gba::Emulator::LoadCallback callback,
    std::optional<kiwi::base::FilePath> backup_path) {
  if (!backup_path) {
    RestorePreviousROM(std::move(previous_rom), resume_old_rom,
                       std::move(callback));
    return;
  }

  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
              "Ignoring invalid GBA save data. Backup: %s",
              backup_path->AsUTF8Unsafe().c_str());
  prepared->save_data.reset();
  LoadPreparedROM(std::move(prepared), std::move(previous_rom), resume_old_rom,
                  std::move(callback));
}

void GBARuntime::RestorePreviousROM(
    PreparedROMPtr previous_rom,
    bool resume_old_rom,
    kiwi::gba::Emulator::LoadCallback callback) {
  if (!previous_rom) {
    current_rom_.reset();
    std::move(callback).Run(false);
    return;
  }

  PreparedROMPtr restored_rom = std::move(previous_rom);
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  kiwi::gba::Emulator::LoadCallback restore_callback = kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
         PreparedROMPtr restored_rom, bool resume_old_rom,
         kiwi::gba::Emulator::LoadCallback callback, bool success) {
        if (weak_lifetime.expired()) {
          return;
        }
        runtime->OnPreviousROMRestored(std::move(restored_rom), resume_old_rom,
                                       std::move(callback), success);
      },
      std::move(weak_lifetime), this, restored_rom, resume_old_rom,
      std::move(callback));
  if (restored_rom->save_data) {
    gba_emulator()->LoadAndRunWithSaveData(*restored_rom->data,
                                           *restored_rom->save_data,
                                           std::move(restore_callback));
  } else {
    gba_emulator()->LoadAndRun(*restored_rom->data,
                               std::move(restore_callback));
  }
}

void GBARuntime::OnPreviousROMRestored(
    PreparedROMPtr previous_rom,
    bool resume_old_rom,
    kiwi::gba::Emulator::LoadCallback callback,
    bool success) {
  if (success) {
    current_rom_ = std::move(previous_rom);
    if (!resume_old_rom) {
      emulator()->Pause();
    }
  } else {
    current_rom_.reset();
  }
  std::move(callback).Run(false);
}

void GBARuntime::OnCurrentROMSavedForUnload(
    bool resume_old_rom,
    kiwi::gba::Emulator::LoadCallback callback,
    bool success) {
  if (!success) {
    if (resume_old_rom) {
      emulator()->Run();
    }
    std::move(callback).Run(false);
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  gba_emulator()->Unload(kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
         kiwi::gba::Emulator::LoadCallback callback) {
        if (weak_lifetime.expired()) {
          return;
        }
        runtime->current_rom_.reset();
        std::move(callback).Run(true);
      },
      std::move(weak_lifetime), this, std::move(callback)));
}

void GBARuntime::OnSaveDataExported(
    std::optional<kiwi::gba::Emulator::SaveDataSnapshot> snapshot) {
  if (!snapshot) {
    OnBatterySaveFlushed(true);
    return;
  }

  kiwi::gba::Emulator::SaveDataSnapshot value = std::move(*snapshot);
  const kiwi::base::FilePath save_path =
      GetSavePath(profile_path(), value.rom_crc32);
  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(&GBARuntime::WriteSaveDataOnIOThread, save_path,
                           value.data),
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
             kiwi::gba::Emulator::SaveDataSnapshot snapshot, bool success) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->OnSaveDataWritten(std::move(snapshot), success);
          },
          std::move(weak_lifetime), this, std::move(value)));
}

void GBARuntime::OnSaveDataWritten(
    kiwi::gba::Emulator::SaveDataSnapshot snapshot,
    bool success) {
  if (!success) {
    OnBatterySaveFlushed(false);
    return;
  }

  if (current_rom_ && GetSavePath(profile_path(), snapshot.rom_crc32) ==
                          current_rom_->save_path) {
    current_rom_->save_data = snapshot.data;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  gba_emulator()->AcknowledgeSaveDataSaved(
      snapshot.rom_crc32, snapshot.generation,
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
             bool success) {
            if (weak_lifetime.expired()) {
              return;
            }
            runtime->OnBatterySaveFlushed(success);
          },
          std::move(weak_lifetime), this));
}

void GBARuntime::OnBatterySaveFlushed(bool success) {
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

void GBARuntime::TriggerDelayedAutoSave(kiwi::base::TimeDelta delta,
                                        GetThumbnailCallback thumbnail,
                                        uint64_t timer_generation) {
  if (!auto_save_started_ || timer_generation != auto_save_timer_generation_) {
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetTimerTaskRunner()->PostDelayedTask(
      FROM_HERE,
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
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

void GBARuntime::RunAutoStateSave(kiwi::base::TimeDelta delta,
                                  GetThumbnailCallback thumbnail,
                                  uint64_t timer_generation) {
  if (!auto_save_started_ || timer_generation != auto_save_timer_generation_) {
    return;
  }

  const kiwi::gba::RomData* rom_data = gba_emulator()->GetRomData();
  if (!emulator()->IsRunning() || !rom_data) {
    TriggerDelayedAutoSave(delta, std::move(thumbnail), timer_generation);
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  gba_emulator()->SaveState(kiwi::base::BindOnce(
      [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
         kiwi::base::TimeDelta delta, GetThumbnailCallback thumbnail,
         uint64_t timer_generation, uint32_t crc32, kiwi::gba::Bytes data) {
        if (weak_lifetime.expired()) {
          return;
        }
        runtime->OnAutoStateCaptured(delta, std::move(thumbnail),
                                     timer_generation, crc32, std::move(data));
      },
      std::move(weak_lifetime), this, delta, std::move(thumbnail),
      timer_generation, rom_data->crc32));
}

void GBARuntime::OnAutoStateCaptured(kiwi::base::TimeDelta delta,
                                     GetThumbnailCallback thumbnail,
                                     uint64_t timer_generation,
                                     uint32_t crc32,
                                     kiwi::gba::Bytes data) {
  if (!auto_save_started_ || timer_generation != auto_save_timer_generation_) {
    return;
  }

  std::weak_ptr<int> weak_lifetime = GetWeakLifetime();
  GetIOTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      kiwi::base::BindOnce(
          &SaveAutoSavedStateOnIOThread,
          static_cast<uint64_t>(std::chrono::system_clock::to_time_t(
              std::chrono::system_clock::now())),
          profile_path(), crc32, std::move(data), thumbnail.Run()),
      kiwi::base::BindOnce(
          [](std::weak_ptr<int> weak_lifetime, GBARuntime* runtime,
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

void GBARuntime::OnAutoStateWritten(kiwi::base::TimeDelta delta,
                                    GetThumbnailCallback thumbnail,
                                    uint64_t timer_generation,
                                    bool success) {
  if (!success) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not auto save GBA state.");
  }
  TriggerDelayedAutoSave(delta, std::move(thumbnail), timer_generation);
}

void GBARuntime::StartAutoStateSave(kiwi::base::TimeDelta delta,
                                    GetThumbnailCallback thumbnail) {
  if (auto_save_started_ || !thumbnail) {
    return;
  }

  auto_save_started_ = true;
  TriggerDelayedAutoSave(delta, std::move(thumbnail),
                         ++auto_save_timer_generation_);
}

void GBARuntime::StopAutoStateSave() {
  auto_save_started_ = false;
  ++auto_save_timer_generation_;
}

void GBARuntime::OnStopPeriodicSaves() {
  StopAutoStateSave();
}
