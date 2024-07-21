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

#ifndef NES_EMULATOR_H_
#define NES_EMULATOR_H_

#include "nes/nes_export.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "nes/debug/debug_port.h"
#include "nes/io_devices.h"

namespace kiwi {
namespace base {
class FilePath;
}

namespace nes {
class DebugPort;
class Configuration;

// The main interface for an emulator. All public methods are thread safe.
class NES_EXPORT Emulator : public base::RefCountedThreadSafe<Emulator>,
                            public Device {
 public:
  friend class base::RefCountedThreadSafe<Emulator>;
  // LoadCallback consists with two parts: ROM's crc32, and whether it is
  // successfully loaded.
  using LoadCallback = base::OnceCallback<void(bool)>;
  using UnloadCallback = base::OnceClosure;
  using ResetCallback = base::OnceClosure;
  using SaveStateCallback = base::OnceCallback<void(Bytes)>;

  enum class RunningState {
    kStopped,
    kPaused,
    kRunning,
  };

  Emulator();

 protected:
  ~Emulator() override;

 public:
  // Initializes the emulator. It will reset CPU, PPU, etc.
  // You have to call this method before run ROM. It will also initialize the
  // environment such as threadpool's task runners.
  virtual void PowerOn() = 0;

  // Power off. The emulator must be power off before destruction. |callback|
  // will be run after power is off.
  virtual void PowerOff() = 0;

  // Loads a ROM. When loads finish, you can call Run() to run the
  // emulator.
  virtual void LoadFromFile(const base::FilePath& rom_path,
                            LoadCallback callback) = 0;
  virtual void LoadFromBinary(const Bytes& data, LoadCallback callback) = 0;

  // Gets currently loaded ROM's data. Returns nullptr if no ROM has been
  // loaded.
  virtual const RomData* GetRomData() = 0;

  // Unloads a ROM.
  virtual void Unload(UnloadCallback callback) = 0;

  // Runs the emulator. Must load cartridge first by calling Load().
  // Emulator will run in a working task runner. Run() only set the running
  // state to kRunning, and you have to call RunOneFrame() per frame.
  virtual void Run() = 0;
  virtual void RunOneFrame() = 0;

  // Pauses the emulator until Run() is called.
  virtual void Pause() = 0;

  // Resets CPU and PPU.
  virtual void Reset(ResetCallback reset_callback) = 0;

  // An utility method to call Load() and Run() in proper thread.
  virtual void LoadAndRun(const base::FilePath& rom_path,
                          LoadCallback = base::DoNothing()) = 0;
  virtual void LoadAndRun(const Bytes& data,
                          LoadCallback = base::DoNothing()) = 0;

  // Steps one CPU cycle. Because Run() will start a working task runner to run
  // cycles, Step() should be called only when the emulator is not running.
  virtual void Step() = 0;

  // Gets the state of the emulator.
  virtual RunningState GetRunningState() = 0;

  // Sets real devices, such as keyboards.
  virtual void SetIODevices(std::unique_ptr<IODevices> io_devices) = 0;
  virtual IODevices* GetIODevices() = 0;

  // Saves or loads current states, such as CPU, PPU, APU, cartridge, etc.
  // If save state failed, an empty data will be returned in |callback|.
  virtual void SaveState(SaveStateCallback callback) = 0;
  virtual void LoadState(const Bytes& data, LoadCallback callback) = 0;

  // Sets or gets emulator's volume. The valid volume is from 0 to 1.
  virtual void SetVolume(float volume) = 0;
  virtual float GetVolume() = 0;

 public:
  virtual void SetDebugPort(DebugPort* debug_port) = 0;

 private:
  // Debug ports. Access these methods only by DebugPort.
  friend class DebugPort;
  virtual PPUContext GetPPUContext() = 0;
  virtual CPUContext GetCPUContext() = 0;
  virtual Byte GetCPUMemory(Address address) = 0;
  virtual Byte GetPPUMemory(Address address) = 0;
  virtual Byte GetOAMMemory(Byte address) = 0;
  virtual Colors GetCurrentFrame() = 0;
  virtual void SetAudioChannelMasks(int audio_channels) = 0;
  virtual int GetAudioChannelMasks() = 0;
};

// Creates an emulator. If |emulate_on_working_thread| is true, emulation will
// be run on a dedicate thread. If |emulate_on_working_thread| is false,
// emulation will be run on UI thread.
NES_EXPORT scoped_refptr<Emulator> CreateEmulator(
    bool emulate_on_working_thread = true);

}  // namespace nes
}  // namespace kiwi

#endif  // NES_EMULATOR_H_
