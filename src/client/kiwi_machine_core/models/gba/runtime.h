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

#ifndef MODELS_GBA_RUNTIME_H_
#define MODELS_GBA_RUNTIME_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "emulator/gba/emulator.h"
#include "models/runtime.h"

class GBARuntime : public Runtime {
 public:
  explicit GBARuntime(
      const kiwi::base::FilePath& profile_path,
      scoped_refptr<kiwi::gba::Emulator> emulator = nullptr,
      scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner = nullptr,
      scoped_refptr<kiwi::base::SequencedTaskRunner> timer_task_runner =
          nullptr);
  ~GBARuntime() override;

  void LoadROM(const kiwi::base::FilePath& rom_path,
               kiwi::emu::Emulator::LoadCallback callback) override;
  void LoadROM(kiwi::emu::Bytes rom_data,
               kiwi::emu::Emulator::LoadCallback callback) override;
  void UnloadROM(kiwi::emu::Emulator::LoadCallback callback) override;
  void FlushBatterySave(kiwi::emu::Emulator::LoadCallback callback) override;
  scoped_refptr<kiwi::emu::Emulator> emulator() override;

  scoped_refptr<kiwi::base::SequencedTaskRunner> GetIOTaskRunner();

 private:
  struct PreparedROM;
  using PreparedROMPtr = std::shared_ptr<PreparedROM>;

  kiwi::gba::Emulator* gba_emulator();
  const kiwi::gba::Emulator* gba_emulator() const;
  static std::optional<PreparedROM> PrepareROMOnIOThread(
      const kiwi::base::FilePath& profile_path,
      const kiwi::base::FilePath& rom_path);
  static std::optional<PreparedROM> PrepareROMDataOnIOThread(
      const kiwi::base::FilePath& profile_path,
      kiwi::gba::Bytes rom_data);
  static bool WriteSaveDataOnIOThread(const kiwi::base::FilePath& save_path,
                                      const kiwi::gba::Bytes& data);

  void OnROMPrepared(kiwi::gba::Emulator::LoadCallback callback,
                     std::optional<PreparedROM> prepared);
  void OnCurrentROMSavedForLoad(PreparedROMPtr prepared,
                                PreparedROMPtr previous_rom,
                                bool resume_old_rom,
                                kiwi::gba::Emulator::LoadCallback callback,
                                bool success);
  void LoadPreparedROM(PreparedROMPtr prepared,
                       PreparedROMPtr previous_rom,
                       bool resume_old_rom,
                       kiwi::gba::Emulator::LoadCallback callback);
  void OnROMLoaded(PreparedROMPtr prepared,
                   PreparedROMPtr previous_rom,
                   bool resume_old_rom,
                   bool had_save_data,
                   kiwi::gba::Emulator::LoadCallback callback,
                   bool success);
  void OnInvalidSaveBackedUp(PreparedROMPtr prepared,
                             PreparedROMPtr previous_rom,
                             bool resume_old_rom,
                             kiwi::gba::Emulator::LoadCallback callback,
                             std::optional<kiwi::base::FilePath> backup_path);
  void RestorePreviousROM(PreparedROMPtr previous_rom,
                          bool resume_old_rom,
                          kiwi::gba::Emulator::LoadCallback callback);
  void OnPreviousROMRestored(PreparedROMPtr previous_rom,
                             bool resume_old_rom,
                             kiwi::gba::Emulator::LoadCallback callback,
                             bool success);
  void OnCurrentROMSavedForUnload(bool resume_old_rom,
                                  kiwi::gba::Emulator::LoadCallback callback,
                                  bool success);
  void StartBatterySaveFlush();
  void OnSaveDataExported(
      std::optional<kiwi::gba::Emulator::SaveDataSnapshot> snapshot);
  void OnSaveDataWritten(kiwi::gba::Emulator::SaveDataSnapshot snapshot,
                         bool success);
  void OnBatterySaveFlushed(bool success);

  scoped_refptr<kiwi::gba::Emulator> emulator_;
  scoped_refptr<kiwi::base::SequencedTaskRunner> io_task_runner_;
  PreparedROMPtr current_rom_;
  bool battery_save_flush_in_progress_ = false;
  bool battery_save_flush_requested_ = false;
  std::vector<kiwi::emu::Emulator::LoadCallback> battery_save_flush_callbacks_;
};

#endif  // MODELS_GBA_RUNTIME_H_
