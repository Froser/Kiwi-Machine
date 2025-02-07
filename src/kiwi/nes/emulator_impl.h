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

#ifndef NES_EMULATOR_IMPL_H_
#define NES_EMULATOR_IMPL_H_

#include <chrono>
#include <set>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "nes/apu.h"
#include "nes/cartridge.h"
#include "nes/controller.h"
#include "nes/cpu_observer.h"
#include "nes/debug/debug_port.h"
#include "nes/emulator.h"
#include "nes/ppu_observer.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class CPU;
class PPUBus;
class CPUBus;
class PPU;
class EmulatorStates;

// Emulator stands for the virtual machine of NES.
class EmulatorImpl : public Emulator, public PPUObserver, public CPUObserver {
 public:
  EmulatorImpl();

 protected:
  ~EmulatorImpl() override;

 public:
  void PowerOn() override;
  void PowerOff() override;
  void LoadFromFile(const base::FilePath& rom_path,
                    LoadCallback callback) override;
  void LoadFromBinary(const Bytes& data, LoadCallback callback) override;
  const RomData* GetRomData() override;
  void Run() override;
  void RunOneFrame() override;
  void Pause() override;
  void LoadAndRun(const base::FilePath& rom_path,
                  LoadCallback callback) override;
  void LoadAndRun(const Bytes& data, LoadCallback callback) override;
  void Unload(UnloadCallback callback) override;
  void Reset(ResetCallback reset_callback) override;
  void Step() override;
  void SetDebugPort(DebugPort* debug_port) override;
  RunningState GetRunningState() override;
  void SetIODevices(std::unique_ptr<IODevices> io_devices) override;
  IODevices* GetIODevices() override;
  void SaveState(SaveStateCallback callback) override;
  void LoadState(const Bytes& data, LoadCallback callback) override;
  void SetVolume(float volume) override;
  float GetVolume() override;
  const Colors& GetLastFrame() override;

  // Device:
  Byte Read(Address address) override;
  void Write(Address address, Byte value) override;

  // PPUObserver:
  void OnPPUStepped() override;
  void OnPPUADDR(Address address) override;
  void OnPPUScanlineStart(int scanline) override;
  void OnPPUScanlineEnd(int scanline) override;
  void OnPPUFrameStart() override;
  void OnPPUFrameEnd() override;
  void OnRenderReady(const Colors& swapbuffer) override;

  // CPUObserver:
  void OnCPUNMI() override;
  void OnCPUBeforeStep(CPUDebugState& state) override;
  void OnCPUStepped() override;

  // Debugging:
  PPUContext GetPPUContext() override;
  CPUContext GetCPUContext() override;
  Byte GetCPUMemory(Address address) override;
  Byte GetPPUMemory(Address address) override;
  Byte GetOAMMemory(Byte address) override;
  Colors GetCurrentFrame() override;
  void SetAudioChannelMasks(int audio_channels) override;
  int GetAudioChannelMasks() override;

  // Operate direct memory access, copy |page| from CPU to PPU.
  // That is, copy 256 bytes from $xx00-$xxFF into OAM via OAMDATA ($2004).
  void DMA(Byte page);

  // Controllers
  void Strobe(Byte strobe);

 public:
  bool is_power_on() { return is_power_on_; }

 private:
  bool LoadFromFileOnProperThread(const base::FilePath& rom_path);
  bool LoadFromBinaryOnProperThread(const Bytes& data);
  bool HandleLoadedResult(Cartridge::LoadResult load_result,
                          scoped_refptr<Cartridge> cartridge);
  void StepInternal();
  void RunOneFrameOnProperThread();
  void PowerOffOnProperThread();
  Bytes SaveStateOnProperThread();
  bool LoadStateOnProperThread(const Bytes& data);
  void ResetOnProperThread();
  void UnloadOnProperThread();
  void PostReset(RunningState last_state);
  void OnIRQFromAPU();
  // EmulatorStates will dump states from emulator, it can access all members.
  friend class EmulatorStates;

 private:
  bool is_power_on_ = false;
  // NTSC NES: 1.789773 MHz (~559 ns per cycle)
  std::unique_ptr<CPU> cpu_;
  std::unique_ptr<CPUBus> cpu_bus_;
  std::unique_ptr<PPU> ppu_;
  std::unique_ptr<PPUBus> ppu_bus_;
  std::unique_ptr<APU> apu_;
  scoped_refptr<Cartridge> cartridge_;
  Controller controller1_;
  Controller controller2_;
  std::atomic<RunningState> running_state_ = RunningState::kStopped;
  std::unique_ptr<IODevices> io_devices_;

  DebugPort* debug_port_ = nullptr;
  scoped_refptr<base::SequencedTaskRunner> emulator_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> render_coroutine_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_EMULATOR_IMPL_H_
