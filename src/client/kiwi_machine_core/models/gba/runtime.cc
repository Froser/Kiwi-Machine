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
#include <zlib.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
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
