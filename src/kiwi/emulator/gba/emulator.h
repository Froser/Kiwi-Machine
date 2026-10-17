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

#ifndef GBA_EMULATOR_H_
#define GBA_EMULATOR_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "emulator/gba/gba_export.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "emulator/emulator.h"
#include "emulator/gba/rom_data.h"
#include "emulator/gba/types.h"

namespace kiwi {
namespace base {
class FilePath;
}

namespace gba {
class IODevices;

// The main interface for a GBA emulator. All public methods are thread safe.
class GBA_EXPORT Emulator : public emu::Emulator {
 public:
  struct SaveDataSnapshot {
    uint32_t rom_crc32 = 0;
    Bytes data;
    uint64_t generation = 0;
  };
  using SaveDataCallback =
      base::OnceCallback<void(std::optional<SaveDataSnapshot>)>;

  struct LoadOptions {
    constexpr LoadOptions(bool should_skip_bios = true,
                          SaveType forced_save_type = SaveType::kAuto)
        : skip_bios(should_skip_bios), save_type(forced_save_type) {}

    bool skip_bios;
    SaveType save_type;
  };

  enum class RunningState {
    kStopped,
    kPaused,
    kRunning,
  };

  Emulator();

 protected:
  virtual ~Emulator();

 public:
  // Initializes the emulator and its execution environment. This method must
  // be called before loading a ROM.
  void PowerOn() override = 0;

  // Powers off the emulator. The emulator must be powered off before
  // destruction.
  void PowerOff() override = 0;

  // Loads a ROM. Run() may be called after the callback reports success.
  virtual void LoadFromFile(const base::FilePath& rom_path,
                            LoadCallback callback,
                            const LoadOptions& options = {}) = 0;
  virtual void LoadFromBinary(const Bytes& data,
                              LoadCallback callback,
                              const LoadOptions& options = {}) = 0;

  // Gets metadata for the loaded ROM, or nullptr when no ROM is loaded.
  virtual const RomData* GetRomData() = 0;

  // Unloads the current ROM.
  virtual void Unload(UnloadCallback callback) = 0;

  // Starts emulation. RunOneFrame() advances emulation by one video frame.
  void Run() override = 0;
  void RunOneFrame() override = 0;

  // Pauses the emulator until Run() is called.
  void Pause() override = 0;

  // Resets the emulated system.
  void Reset(ResetCallback reset_callback) override = 0;

  // Loads a ROM and starts it on the appropriate execution sequence.
  virtual void LoadAndRun(const base::FilePath& rom_path,
                          LoadCallback callback = base::DoNothing(),
                          const LoadOptions& options = {}) = 0;
  virtual void LoadAndRun(const Bytes& data,
                          LoadCallback callback = base::DoNothing(),
                          const LoadOptions& options = {}) = 0;

  // Advances the emulated processor by one execution step. This method should
  // only be called while the emulator is not running.
  virtual void Step() = 0;

  virtual RunningState GetRunningState() = 0;
  bool IsRunning() override {
    return GetRunningState() == RunningState::kRunning;
  }
  bool IsPaused() override {
    return GetRunningState() == RunningState::kPaused;
  }
  bool HasROM() override { return GetRomData() != nullptr; }

  // Sets or gets the devices used for input, rendering, and audio output.
  virtual void SetIODevices(std::unique_ptr<IODevices> io_devices) = 0;
  virtual IODevices* GetIODevices() = 0;

  // Saves or loads the complete emulation state. SaveState() returns an empty
  // byte array when serialization fails.
  virtual void SaveState(SaveStateCallback callback) = 0;
  virtual void LoadState(const Bytes& data, LoadCallback callback) = 0;

  // Sets or gets the output volume. Valid values range from 0 to 1.
  void SetVolume(float volume) override = 0;
  float GetVolume() override = 0;

  // Gets the last completed frame or the frame currently being produced.
  virtual const Colors& GetLastFrame() = 0;
  virtual const Colors& GetCurrentFrame() = 0;

  // Loads an optional real GBA BIOS. Without one, mGBA's HLE BIOS is used.
  virtual void LoadBIOSFromFile(const base::FilePath& bios_path,
                                LoadCallback callback) = 0;
  virtual void LoadBIOSFromBinary(const Bytes& data, LoadCallback callback) = 0;
  virtual bool HasBIOS() = 0;

  // Loads a ROM with its battery-backed save data and starts emulation.
  virtual void LoadAndRunWithSaveData(const Bytes& data,
                                      const Bytes& initial_save_data,
                                      LoadCallback callback = base::DoNothing(),
                                      const LoadOptions& options = {}) = 0;

  // Exports battery-backed save data. If |dirty_only| is true, no snapshot is
  // returned until data differs from the last acknowledged generation.
  virtual void ExportSaveData(bool dirty_only, SaveDataCallback callback) = 0;
  virtual void AcknowledgeSaveDataSaved(uint32_t rom_crc32,
                                        uint64_t generation,
                                        LoadCallback callback) = 0;
};

// Creates a GBA emulator backed by mGBA.
GBA_EXPORT scoped_refptr<Emulator> CreateEmulator();

// Creates an emulator for tests running on the current task sequence.
GBA_EXPORT scoped_refptr<Emulator> CreateEmulatorForTesting();

}  // namespace gba
}  // namespace kiwi

#endif  // GBA_EMULATOR_H_
