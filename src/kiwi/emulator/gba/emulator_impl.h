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

#ifndef GBA_EMULATOR_IMPL_H_
#define GBA_EMULATOR_IMPL_H_

#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "emulator/gba/emulator.h"
#include "emulator/gba/io_devices.h"

struct mCore;
struct VFile;

namespace kiwi {
namespace gba {

// EmulatorImpl adapts the mGBA core to Kiwi's emulator interface.
class EmulatorImpl : public Emulator {
 public:
  EmulatorImpl();

 protected:
  ~EmulatorImpl() override;

 public:
  void PowerOn() override;
  void PowerOff() override;
  void LoadFromFile(const base::FilePath& rom_path,
                    LoadCallback callback,
                    const LoadOptions& options) override;
  void LoadFromBinary(const Bytes& data,
                      LoadCallback callback,
                      const LoadOptions& options) override;
  const RomData* GetRomData() override;
  void Unload(UnloadCallback callback) override;
  void Run() override;
  void RunOneFrame() override;
  void Pause() override;
  void Reset(ResetCallback reset_callback) override;
  void LoadAndRun(const base::FilePath& rom_path,
                  LoadCallback callback,
                  const LoadOptions& options) override;
  void LoadAndRun(const Bytes& data,
                  LoadCallback callback,
                  const LoadOptions& options) override;
  void Step() override;
  RunningState GetRunningState() override;
  void SetIODevices(std::unique_ptr<IODevices> io_devices) override;
  IODevices* GetIODevices() override;
  void SaveState(SaveStateCallback callback) override;
  void LoadState(const Bytes& data, LoadCallback callback) override;
  void SetVolume(float volume) override;
  float GetVolume() override;
  const Colors& GetLastFrame() override;
  const Colors& GetCurrentFrame() override;
  void LoadBIOSFromFile(const base::FilePath& bios_path,
                        LoadCallback callback) override;
  void LoadBIOSFromBinary(const Bytes& data, LoadCallback callback) override;
  bool HasBIOS() override;
  void LoadAndRunWithSaveData(const Bytes& data,
                              const Bytes& initial_save_data,
                              LoadCallback callback,
                              const LoadOptions& options) override;
  void ExportSaveData(bool dirty_only, SaveDataCallback callback) override;
  void AcknowledgeSaveDataSaved(uint32_t rom_crc32,
                                uint64_t generation,
                                LoadCallback callback) override;
  void GetDebugSnapshot(size_t instruction_count,
                        DebugSnapshotCallback callback) override;
  void StepAndGetDebugSnapshot(size_t instruction_count,
                               DebugSnapshotCallback callback) override;
  void GetDebugMemorySnapshot(uint32_t address,
                              size_t byte_count,
                              DebugMemorySnapshotCallback callback) override;
  void GetDebugGraphicsSnapshot(
      DebugGraphicsSnapshotCallback callback) override;
  void GetDebugAudioSnapshot(DebugAudioSnapshotCallback callback) override;
  void SetDebugVideoLayerEnabled(size_t id,
                                 bool enabled,
                                 DebugResultCallback callback) override;
  void SetDebugAudioChannelEnabled(size_t id,
                                   bool enabled,
                                   DebugResultCallback callback) override;
  void AddDebugBreakpoint(uint32_t address,
                          DebugResultCallback callback) override;
  void AddDebugWatchpoint(uint32_t address,
                          DebugWatchpointType type,
                          DebugResultCallback callback) override;
  void RemoveDebugPoint(int64_t id, DebugResultCallback callback) override;
  void SetDebugStackTraceEnabled(bool enabled,
                                 DebugResultCallback callback) override;
  void SetDebugBreakCallback(base::RepeatingClosure on_debug_break) override;
  DebugMetrics GetDebugMetrics() const override;

  bool is_power_on() const { return is_power_on_; }

 private:
  struct CoreAdapters;
  struct CoreDebugger;

  void LoadFromBinaryWithOptionalSaveData(
      const Bytes& data,
      LoadCallback callback,
      const LoadOptions& options,
      std::optional<Bytes> initial_save_data);
  bool LoadFromFileOnProperThread(const base::FilePath& rom_path,
                                  const LoadOptions& options);
  bool LoadFromBinaryOnProperThread(
      const Bytes& data,
      const LoadOptions& options,
      const std::optional<Bytes>& initial_save_data);
  bool LoadROMOnProperThread(VFile* rom,
                             const LoadOptions& options,
                             const std::optional<Bytes>& initial_save_data);
  bool LoadBIOSFromFileOnProperThread(const base::FilePath& bios_path);
  bool LoadBIOSFromBinaryOnProperThread(const Bytes& data);
  bool LoadBIOSOnProperThread(VFile* bios);
  bool ImportSaveDataOnProperThread(const Bytes& data);
  std::optional<SaveDataSnapshot> ExportSaveDataOnProperThread(bool dirty_only);
  bool AcknowledgeSaveDataSavedOnProperThread(uint32_t rom_crc32,
                                              uint64_t generation);
  void PopulateRomDataOnProperThread();
  void UpdateSaveDataMetadataOnProperThread();
  void ResetSaveDataTrackingOnProperThread();
  void UnloadOnProperThread();
  void ResetOnProperThread();
  void RunOneFrameOnProperThread();
  void StepOnProperThread();
  void PowerOffOnProperThread();
  Bytes SaveStateOnProperThread();
  bool LoadStateOnProperThread(const Bytes& data);
  void UpdateInputOnProperThread();
  void PresentFrameOnProperThread();
  void DrainAudioOnProperThread();
  void ClearFrameBuffersOnProperThread();
  std::optional<DebugSnapshot> GetDebugSnapshotOnProperThread(
      size_t instruction_count);
  std::optional<DebugSnapshot> StepAndGetDebugSnapshotOnProperThread(
      size_t instruction_count);
  std::optional<DebugMemorySnapshot> GetDebugMemorySnapshotOnProperThread(
      uint32_t address,
      size_t byte_count);
  std::optional<DebugGraphicsSnapshot> GetDebugGraphicsSnapshotOnProperThread();
  std::optional<DebugAudioSnapshot> GetDebugAudioSnapshotOnProperThread();
  bool SetDebugVideoLayerEnabledOnProperThread(size_t id, bool enabled);
  bool SetDebugAudioChannelEnabledOnProperThread(size_t id, bool enabled);
  bool AddDebugBreakpointOnProperThread(uint32_t address);
  bool AddDebugWatchpointOnProperThread(uint32_t address,
                                        DebugWatchpointType type);
  bool RemoveDebugPointOnProperThread(int64_t id);
  bool SetDebugStackTraceEnabledOnProperThread(bool enabled);
  bool EnsureDebuggerOnProperThread();
  void PopulateDebugStateOnProperThread(DebugSnapshot* snapshot);
  void ClearDebugPointsOnProperThread();
  void OnDebuggerEnteredOnProperThread(int reason, const void* entry_info);

  bool RunsTasksInCurrentSequence() const;

  std::atomic_bool is_power_on_ = false;
  std::atomic_bool is_rom_loaded_ = false;
  std::atomic_bool has_bios_ = false;
  std::atomic<RunningState> running_state_ = RunningState::kStopped;
  std::atomic<float> volume_ = 1.f;

  mCore* core_ = nullptr;
  bool config_initialized_ = false;
  std::unique_ptr<CoreAdapters> core_adapters_;
  std::unique_ptr<CoreDebugger> core_debugger_;
  std::unique_ptr<IODevices> io_devices_;
  std::unique_ptr<RomData> rom_data_;

  std::array<Colors, 2> frame_buffers_;
  size_t current_frame_index_ = 0;
  size_t last_frame_index_ = 1;
  std::vector<Sample> audio_samples_;
  Bytes observed_save_data_;
  uint64_t save_data_generation_ = 0;
  uint64_t persisted_save_data_generation_ = 0;
  std::array<bool, 8> debug_video_layers_enabled_ = {true, true, true, true,
                                                     true, true, true, true};
  std::array<bool, 6> debug_audio_channels_enabled_ = {true, true, true,
                                                       true, true, true};
  DebugBreakEvent last_debug_break_;
  base::RepeatingClosure debug_break_callback_;
  scoped_refptr<base::SequencedTaskRunner> debug_break_task_runner_;
  std::atomic<uint64_t> debug_frame_count_ = 0;
  std::atomic<uint32_t> debug_last_frame_duration_us_ = 0;
  std::atomic<uint32_t> debug_last_audio_sample_count_ = 0;

  scoped_refptr<base::SequencedTaskRunner> emulator_task_runner_;
};

}  // namespace gba
}  // namespace kiwi

#endif  // GBA_EMULATOR_IMPL_H_
