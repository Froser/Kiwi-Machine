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

#include "emulator/gba/emulator_impl.h"

#include <fcntl.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#undef LIKELY
#undef UNLIKELY

#include "third_party/mgba-0.10.5/include/mgba-util/vfs.h"
#include "third_party/mgba-0.10.5/include/mgba/core/blip_buf.h"
#include "third_party/mgba-0.10.5/include/mgba/core/core.h"
#include "third_party/mgba-0.10.5/include/mgba/core/serialize.h"
#include "third_party/mgba-0.10.5/include/mgba/core/timing.h"
#include "third_party/mgba-0.10.5/include/mgba/debugger/debugger.h"
#include "third_party/mgba-0.10.5/include/mgba/gba/interface.h"
#include "third_party/mgba-0.10.5/include/mgba/internal/arm/arm.h"
#include "third_party/mgba-0.10.5/include/mgba/internal/arm/decoder.h"
#include "third_party/mgba-0.10.5/include/mgba/internal/debugger/stack-trace.h"
#include "third_party/mgba-0.10.5/include/mgba/internal/gba/gba.h"
#include "third_party/mgba-0.10.5/include/mgba/internal/gba/input.h"
#include "third_party/mgba-0.10.5/include/mgba/internal/gba/io.h"
#include "third_party/mgba-0.10.5/include/mgba/internal/gba/savedata.h"
#include "third_party/mgba-0.10.5/include/mgba/internal/gba/video.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace kiwi {
namespace gba {
namespace {

constexpr size_t kAudioBufferSamples = 2048;
constexpr size_t kMaxDebugInstructionCount = 256;
constexpr size_t kMaxDebugMemoryBytes = 4096;
constexpr Color kOpaqueAlpha = 0xff000000;
constexpr Color kColorMask = 0x00ffffff;

size_t GetSaveTypeSize(SaveType type) {
  switch (type) {
    case SaveType::kSRAM:
      return SIZE_CART_SRAM;
    case SaveType::kSRAM512:
      return SIZE_CART_SRAM512;
    case SaveType::kFlash512:
      return SIZE_CART_FLASH512;
    case SaveType::kFlash1M:
      return SIZE_CART_FLASH1M;
    case SaveType::kEEPROM:
      return SIZE_CART_EEPROM;
    case SaveType::kEEPROM512:
      return SIZE_CART_EEPROM512;
    case SaveType::kAuto:
    case SaveType::kNone:
      return 0;
  }
  return 0;
}

SavedataType ToMGBA(SaveType type) {
  switch (type) {
    case SaveType::kNone:
      return SAVEDATA_FORCE_NONE;
    case SaveType::kSRAM:
      return SAVEDATA_SRAM;
    case SaveType::kSRAM512:
      return SAVEDATA_SRAM512;
    case SaveType::kFlash512:
      return SAVEDATA_FLASH512;
    case SaveType::kFlash1M:
      return SAVEDATA_FLASH1M;
    case SaveType::kEEPROM:
      return SAVEDATA_EEPROM;
    case SaveType::kEEPROM512:
      return SAVEDATA_EEPROM512;
    case SaveType::kAuto:
      return SAVEDATA_AUTODETECT;
  }
  return SAVEDATA_AUTODETECT;
}

SaveType FromMGBA(SavedataType type) {
  switch (type) {
    case SAVEDATA_FORCE_NONE:
      return SaveType::kNone;
    case SAVEDATA_SRAM:
      return SaveType::kSRAM;
    case SAVEDATA_SRAM512:
      return SaveType::kSRAM512;
    case SAVEDATA_FLASH512:
      return SaveType::kFlash512;
    case SAVEDATA_FLASH1M:
      return SaveType::kFlash1M;
    case SAVEDATA_EEPROM:
      return SaveType::kEEPROM;
    case SAVEDATA_EEPROM512:
      return SaveType::kEEPROM512;
    case SAVEDATA_AUTODETECT:
      return SaveType::kAuto;
  }
  return SaveType::kAuto;
}

SaveType InferSaveType(size_t size) {
  switch (size) {
    case SIZE_CART_SRAM:
      return SaveType::kSRAM;
    case SIZE_CART_SRAM512:
      return SaveType::kFlash512;
    case SIZE_CART_FLASH1M:
      return SaveType::kFlash1M;
    case SIZE_CART_EEPROM:
      return SaveType::kEEPROM;
    case SIZE_CART_EEPROM512:
      return SaveType::kEEPROM512;
    default:
      return SaveType::kAuto;
  }
}

std::string TrimROMString(const char* value, size_t size) {
  size_t end = 0;
  while (end < size && value[end])
    ++end;
  while (end > 0 && value[end - 1] == ' ')
    --end;
  return std::string(value, end);
}

uint32_t ToMGBAKeyMask(ControllerButton button) {
  switch (button) {
    case ControllerButton::kA:
      return 1u << GBA_KEY_A;
    case ControllerButton::kB:
      return 1u << GBA_KEY_B;
    case ControllerButton::kSelect:
      return 1u << GBA_KEY_SELECT;
    case ControllerButton::kStart:
      return 1u << GBA_KEY_START;
    case ControllerButton::kUp:
      return 1u << GBA_KEY_UP;
    case ControllerButton::kDown:
      return 1u << GBA_KEY_DOWN;
    case ControllerButton::kLeft:
      return 1u << GBA_KEY_LEFT;
    case ControllerButton::kRight:
      return 1u << GBA_KEY_RIGHT;
    case ControllerButton::kL:
      return 1u << GBA_KEY_L;
    case ControllerButton::kR:
      return 1u << GBA_KEY_R;
    case ControllerButton::kMax:
      break;
  }
  return 0;
}

enum mWatchpointType ToMGBAWatchpointType(Emulator::DebugWatchpointType type) {
  switch (type) {
    case Emulator::DebugWatchpointType::kWrite:
      return WATCHPOINT_WRITE;
    case Emulator::DebugWatchpointType::kRead:
      return WATCHPOINT_READ;
    case Emulator::DebugWatchpointType::kReadWrite:
      return WATCHPOINT_RW;
    case Emulator::DebugWatchpointType::kWriteChange:
      return WATCHPOINT_WRITE_CHANGE;
  }
  return WATCHPOINT_WRITE;
}

Emulator::DebugWatchpointType FromMGBAWatchpointType(
    enum mWatchpointType type) {
  switch (type) {
    case WATCHPOINT_READ:
      return Emulator::DebugWatchpointType::kRead;
    case WATCHPOINT_RW:
      return Emulator::DebugWatchpointType::kReadWrite;
    case WATCHPOINT_CHANGE:
    case WATCHPOINT_WRITE_CHANGE:
      return Emulator::DebugWatchpointType::kWriteChange;
    case WATCHPOINT_WRITE:
      return Emulator::DebugWatchpointType::kWrite;
  }
  return Emulator::DebugWatchpointType::kWrite;
}

void RunAfterLoad(scoped_refptr<EmulatorImpl> emulator,
                  Emulator::LoadCallback callback,
                  bool success) {
  if (success) {
    emulator->Run();
  } else {
    LOG(ERROR) << "Failed to load GBA ROM.";
  }
  std::move(callback).Run(success);
}

}  // namespace

struct EmulatorImpl::CoreAdapters {
  struct Rotation {
    mRotationSource source = {};
    EmulatorImpl* emulator = nullptr;

    static Rotation* From(mRotationSource* source) {
      return reinterpret_cast<Rotation*>(source);
    }

    static IODevices::MotionDevice* Device(mRotationSource* source) {
      IODevices* io_devices = From(source)->emulator->GetIODevices();
      return io_devices ? io_devices->motion_device() : nullptr;
    }

    static void Sample(mRotationSource* source) {
      if (IODevices::MotionDevice* device = Device(source))
        device->Sample();
    }

    static int32_t ReadTiltX(mRotationSource* source) {
      IODevices::MotionDevice* device = Device(source);
      return device ? device->ReadTiltX() : 0;
    }

    static int32_t ReadTiltY(mRotationSource* source) {
      IODevices::MotionDevice* device = Device(source);
      return device ? device->ReadTiltY() : 0;
    }

    static int32_t ReadGyroZ(mRotationSource* source) {
      IODevices::MotionDevice* device = Device(source);
      return device ? device->ReadGyroZ() : 0;
    }
  };

  struct Rumble {
    mRumble source = {};
    EmulatorImpl* emulator = nullptr;

    static Rumble* From(mRumble* source) {
      return reinterpret_cast<Rumble*>(source);
    }

    static void SetRumble(mRumble* source, int enabled) {
      IODevices* io_devices = From(source)->emulator->GetIODevices();
      if (io_devices && io_devices->rumble_device())
        io_devices->rumble_device()->SetRumble(enabled != 0);
    }
  };

  struct Luminance {
    GBALuminanceSource source = {};
    EmulatorImpl* emulator = nullptr;

    static Luminance* From(GBALuminanceSource* source) {
      return reinterpret_cast<Luminance*>(source);
    }

    static void Sample(GBALuminanceSource* source) {
      IODevices* io_devices = From(source)->emulator->GetIODevices();
      if (io_devices && io_devices->luminance_device())
        io_devices->luminance_device()->Sample();
    }

    static uint8_t ReadLuminance(GBALuminanceSource* source) {
      IODevices* io_devices = From(source)->emulator->GetIODevices();
      return io_devices && io_devices->luminance_device()
                 ? io_devices->luminance_device()->ReadLuminance()
                 : 0xff;
    }
  };

  struct RTC {
    mRTCSource source = {};
    EmulatorImpl* emulator = nullptr;

    static RTC* From(mRTCSource* source) {
      return reinterpret_cast<RTC*>(source);
    }

    static void Sample(mRTCSource* source) {
      IODevices* io_devices = From(source)->emulator->GetIODevices();
      if (io_devices && io_devices->rtc_device())
        io_devices->rtc_device()->Sample();
    }

    static time_t UnixTime(mRTCSource* source) {
      IODevices* io_devices = From(source)->emulator->GetIODevices();
      if (io_devices && io_devices->rtc_device()) {
        return static_cast<time_t>(
            io_devices->rtc_device()->GetUnixTimeSeconds());
      }
      return std::time(nullptr);
    }
  };

  explicit CoreAdapters(EmulatorImpl* emulator) {
    rotation.emulator = emulator;
    rotation.source.sample = &Rotation::Sample;
    rotation.source.readTiltX = &Rotation::ReadTiltX;
    rotation.source.readTiltY = &Rotation::ReadTiltY;
    rotation.source.readGyroZ = &Rotation::ReadGyroZ;

    rumble.emulator = emulator;
    rumble.source.setRumble = &Rumble::SetRumble;

    luminance.emulator = emulator;
    luminance.source.sample = &Luminance::Sample;
    luminance.source.readLuminance = &Luminance::ReadLuminance;

    rtc.emulator = emulator;
    rtc.source.sample = &RTC::Sample;
    rtc.source.unixTime = &RTC::UnixTime;
  }

  Rotation rotation;
  Rumble rumble;
  Luminance luminance;
  RTC rtc;
};

struct EmulatorImpl::CoreDebugger {
  explicit CoreDebugger(EmulatorImpl* emulator) : emulator(emulator) {
    debugger.type = DEBUGGER_CUSTOM;
    debugger.paused = &CoreDebugger::Paused;
    debugger.entered = &CoreDebugger::Entered;
  }

  static CoreDebugger* From(mDebugger* debugger) {
    return reinterpret_cast<CoreDebugger*>(debugger);
  }

  static void Paused(mDebugger* debugger) {}

  static void Entered(mDebugger* debugger,
                      enum mDebuggerEntryReason reason,
                      mDebuggerEntryInfo* info) {
    From(debugger)->emulator->OnDebuggerEnteredOnProperThread(reason, info);
  }

  mDebugger debugger = {};
  EmulatorImpl* emulator = nullptr;
};

EmulatorImpl::EmulatorImpl() {
  for (Colors& frame : frame_buffers_)
    frame.resize(kFrameWidth * kFrameHeight, kOpaqueAlpha);
}

EmulatorImpl::~EmulatorImpl() {
  DCHECK(!core_) << "PowerOff() must be called before destruction.";
}

void EmulatorImpl::PowerOn() {
  if (is_power_on_)
    return;

  CHECK(base::SingleThreadTaskRunner::HasCurrentDefault());
  emulator_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  core_ = mCoreCreate(mPLATFORM_GBA);
  if (!core_) {
    LOG(ERROR) << "Failed to create mGBA core.";
    return;
  }

  if (!core_->init(core_)) {
    LOG(ERROR) << "Failed to initialize mGBA core.";
    std::free(core_);
    core_ = nullptr;
    return;
  }

  mCoreInitConfig(core_, "kiwi");
  config_initialized_ = true;
  core_->opts.skipBios = true;
  core_->opts.useBios = false;
  core_->opts.volume = 0x100;
  core_->opts.sampleRate = IODevices::AudioDevice::kFrequency;
  core_->opts.audioBuffers = kAudioBufferSamples;
  core_->loadConfig(core_, &core_->config);

  core_->setAudioBufferSize(core_, kAudioBufferSamples);
  blip_set_rates(core_->getAudioChannel(core_, 0), core_->frequency(core_),
                 IODevices::AudioDevice::kFrequency);
  blip_set_rates(core_->getAudioChannel(core_, 1), core_->frequency(core_),
                 IODevices::AudioDevice::kFrequency);

  core_adapters_ = std::make_unique<CoreAdapters>(this);
  core_->setPeripheral(core_, mPERIPH_ROTATION,
                       &core_adapters_->rotation.source);
  core_->setPeripheral(core_, mPERIPH_RUMBLE, &core_adapters_->rumble.source);
  core_->setPeripheral(core_, mPERIPH_GBA_LUMINANCE,
                       &core_adapters_->luminance.source);
  mCoreSetRTC(core_, &core_adapters_->rtc.source);

  debug_video_layers_enabled_.fill(true);
  debug_audio_channels_enabled_.fill(true);
  last_debug_break_ = {};
  debug_frame_count_ = 0;
  debug_last_frame_duration_us_ = 0;
  debug_last_audio_sample_count_ = 0;
  ClearFrameBuffersOnProperThread();
  is_power_on_ = true;
}

void EmulatorImpl::PowerOff() {
  if (!is_power_on_)
    return;

  running_state_ = RunningState::kStopped;
  if (RunsTasksInCurrentSequence()) {
    PowerOffOnProperThread();
    return;
  }

  emulator_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&EmulatorImpl::PowerOffOnProperThread,
                                base::RetainedRef(this)));
}

void EmulatorImpl::LoadFromFile(const base::FilePath& rom_path,
                                LoadCallback callback,
                                const LoadOptions& options) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(LoadFromFileOnProperThread(rom_path, options));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::LoadFromFileOnProperThread,
                     base::RetainedRef(this), rom_path, options),
      std::move(callback));
}

void EmulatorImpl::LoadFromBinary(const Bytes& data,
                                  LoadCallback callback,
                                  const LoadOptions& options) {
  LoadFromBinaryWithOptionalSaveData(data, std::move(callback), options,
                                     std::nullopt);
}

void EmulatorImpl::LoadFromBinaryWithOptionalSaveData(
    const Bytes& data,
    LoadCallback callback,
    const LoadOptions& options,
    std::optional<Bytes> initial_save_data) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(
        LoadFromBinaryOnProperThread(data, options, initial_save_data));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::LoadFromBinaryOnProperThread,
                     base::RetainedRef(this), data, options,
                     std::move(initial_save_data)),
      std::move(callback));
}

const RomData* EmulatorImpl::GetRomData() {
  return rom_data_.get();
}

void EmulatorImpl::Unload(UnloadCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run();
    return;
  }

  running_state_ = RunningState::kStopped;
  if (RunsTasksInCurrentSequence()) {
    UnloadOnProperThread();
    std::move(callback).Run();
    return;
  }

  emulator_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::UnloadOnProperThread,
                     base::RetainedRef(this)),
      std::move(callback));
}

void EmulatorImpl::Run() {
  if (!is_rom_loaded_) {
    LOG(ERROR) << "ROM has not been loaded.";
    return;
  }
  if (core_debugger_)
    core_debugger_->debugger.state = DEBUGGER_RUNNING;
  running_state_ = RunningState::kRunning;
}

void EmulatorImpl::RunOneFrame() {
  if (!is_power_on_)
    return;

  if (RunsTasksInCurrentSequence()) {
    RunOneFrameOnProperThread();
    return;
  }

  emulator_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&EmulatorImpl::RunOneFrameOnProperThread,
                                base::RetainedRef(this)));
}

void EmulatorImpl::Pause() {
  RunningState expected = RunningState::kRunning;
  running_state_.compare_exchange_strong(expected, RunningState::kPaused);
}

void EmulatorImpl::Reset(ResetCallback reset_callback) {
  if (!is_power_on_) {
    std::move(reset_callback).Run();
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    ResetOnProperThread();
    std::move(reset_callback).Run();
    return;
  }

  emulator_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::ResetOnProperThread,
                     base::RetainedRef(this)),
      std::move(reset_callback));
}

void EmulatorImpl::LoadAndRun(const base::FilePath& rom_path,
                              LoadCallback callback,
                              const LoadOptions& options) {
  LoadFromFile(rom_path,
               base::BindOnce(&RunAfterLoad, base::RetainedRef(this),
                              std::move(callback)),
               options);
}

void EmulatorImpl::LoadAndRun(const Bytes& data,
                              LoadCallback callback,
                              const LoadOptions& options) {
  LoadFromBinary(data,
                 base::BindOnce(&RunAfterLoad, base::RetainedRef(this),
                                std::move(callback)),
                 options);
}

void EmulatorImpl::Step() {
  if (!is_power_on_ || !is_rom_loaded_)
    return;

  DCHECK(running_state_ != RunningState::kRunning)
      << "Step() should only be called while the emulator is paused.";
  if (running_state_ == RunningState::kRunning)
    return;

  if (RunsTasksInCurrentSequence()) {
    StepOnProperThread();
    return;
  }

  emulator_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&EmulatorImpl::StepOnProperThread,
                                base::RetainedRef(this)));
}

Emulator::RunningState EmulatorImpl::GetRunningState() {
  return running_state_;
}

void EmulatorImpl::SetIODevices(std::unique_ptr<IODevices> io_devices) {
  if (io_devices_ && io_devices_->rumble_device())
    io_devices_->rumble_device()->SetRumble(false);
  io_devices_ = std::move(io_devices);
}

IODevices* EmulatorImpl::GetIODevices() {
  return io_devices_.get();
}

void EmulatorImpl::SaveState(SaveStateCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run({});
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(SaveStateOnProperThread());
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::SaveStateOnProperThread,
                     base::RetainedRef(this)),
      std::move(callback));
}

void EmulatorImpl::LoadState(const Bytes& data, LoadCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(LoadStateOnProperThread(data));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::LoadStateOnProperThread,
                     base::RetainedRef(this), data),
      std::move(callback));
}

void EmulatorImpl::SetVolume(float volume) {
  volume_ = std::clamp(volume, 0.f, 1.f);
}

float EmulatorImpl::GetVolume() {
  return volume_;
}

const Colors& EmulatorImpl::GetLastFrame() {
  return frame_buffers_[last_frame_index_];
}

const Colors& EmulatorImpl::GetCurrentFrame() {
  return frame_buffers_[current_frame_index_];
}

void EmulatorImpl::LoadBIOSFromFile(const base::FilePath& bios_path,
                                    LoadCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(LoadBIOSFromFileOnProperThread(bios_path));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::LoadBIOSFromFileOnProperThread,
                     base::RetainedRef(this), bios_path),
      std::move(callback));
}

void EmulatorImpl::LoadBIOSFromBinary(const Bytes& data,
                                      LoadCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(LoadBIOSFromBinaryOnProperThread(data));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::LoadBIOSFromBinaryOnProperThread,
                     base::RetainedRef(this), data),
      std::move(callback));
}

bool EmulatorImpl::HasBIOS() {
  return has_bios_;
}

void EmulatorImpl::LoadAndRunWithSaveData(const Bytes& data,
                                          const Bytes& initial_save_data,
                                          LoadCallback callback,
                                          const LoadOptions& options) {
  LoadCallback load_callback = base::BindOnce(
      &RunAfterLoad, base::RetainedRef(this), std::move(callback));
  LoadFromBinaryWithOptionalSaveData(
      data, std::move(load_callback), options,
      initial_save_data.empty() ? std::nullopt
                                : std::optional<Bytes>(initial_save_data));
}

void EmulatorImpl::ExportSaveData(bool dirty_only, SaveDataCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(ExportSaveDataOnProperThread(dirty_only));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::ExportSaveDataOnProperThread,
                     base::RetainedRef(this), dirty_only),
      std::move(callback));
}

void EmulatorImpl::AcknowledgeSaveDataSaved(uint32_t rom_crc32,
                                            uint64_t generation,
                                            LoadCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(
        AcknowledgeSaveDataSavedOnProperThread(rom_crc32, generation));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::AcknowledgeSaveDataSavedOnProperThread,
                     base::RetainedRef(this), rom_crc32, generation),
      std::move(callback));
}

void EmulatorImpl::GetDebugSnapshot(size_t instruction_count,
                                    DebugSnapshotCallback callback) {
  if (!is_power_on_ || !is_rom_loaded_ ||
      running_state_ == RunningState::kRunning) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(GetDebugSnapshotOnProperThread(instruction_count));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::GetDebugSnapshotOnProperThread,
                     base::RetainedRef(this), instruction_count),
      std::move(callback));
}

void EmulatorImpl::StepAndGetDebugSnapshot(size_t instruction_count,
                                           DebugSnapshotCallback callback) {
  if (!is_power_on_ || !is_rom_loaded_ ||
      running_state_ == RunningState::kRunning) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(
        StepAndGetDebugSnapshotOnProperThread(instruction_count));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::StepAndGetDebugSnapshotOnProperThread,
                     base::RetainedRef(this), instruction_count),
      std::move(callback));
}

void EmulatorImpl::GetDebugMemorySnapshot(
    uint32_t address,
    size_t byte_count,
    DebugMemorySnapshotCallback callback) {
  if (!is_power_on_ || !is_rom_loaded_ ||
      running_state_ == RunningState::kRunning) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(
        GetDebugMemorySnapshotOnProperThread(address, byte_count));
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::GetDebugMemorySnapshotOnProperThread,
                     base::RetainedRef(this), address, byte_count),
      std::move(callback));
}

void EmulatorImpl::GetDebugGraphicsSnapshot(
    DebugGraphicsSnapshotCallback callback) {
  if (!is_power_on_ || !is_rom_loaded_ ||
      running_state_ == RunningState::kRunning) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(GetDebugGraphicsSnapshotOnProperThread());
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::GetDebugGraphicsSnapshotOnProperThread,
                     base::RetainedRef(this)),
      std::move(callback));
}

void EmulatorImpl::GetDebugAudioSnapshot(DebugAudioSnapshotCallback callback) {
  if (!is_power_on_ || !is_rom_loaded_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(GetDebugAudioSnapshotOnProperThread());
    return;
  }

  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::GetDebugAudioSnapshotOnProperThread,
                     base::RetainedRef(this)),
      std::move(callback));
}

void EmulatorImpl::SetDebugVideoLayerEnabled(size_t id,
                                             bool enabled,
                                             DebugResultCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }
  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(
        SetDebugVideoLayerEnabledOnProperThread(id, enabled));
    return;
  }
  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::SetDebugVideoLayerEnabledOnProperThread,
                     base::RetainedRef(this), id, enabled),
      std::move(callback));
}

void EmulatorImpl::SetDebugAudioChannelEnabled(size_t id,
                                               bool enabled,
                                               DebugResultCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }
  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(
        SetDebugAudioChannelEnabledOnProperThread(id, enabled));
    return;
  }
  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::SetDebugAudioChannelEnabledOnProperThread,
                     base::RetainedRef(this), id, enabled),
      std::move(callback));
}

void EmulatorImpl::AddDebugBreakpoint(uint32_t address,
                                      DebugResultCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }
  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(AddDebugBreakpointOnProperThread(address));
    return;
  }
  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::AddDebugBreakpointOnProperThread,
                     base::RetainedRef(this), address),
      std::move(callback));
}

void EmulatorImpl::AddDebugWatchpoint(uint32_t address,
                                      DebugWatchpointType type,
                                      DebugResultCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }
  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(AddDebugWatchpointOnProperThread(address, type));
    return;
  }
  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::AddDebugWatchpointOnProperThread,
                     base::RetainedRef(this), address, type),
      std::move(callback));
}

void EmulatorImpl::RemoveDebugPoint(int64_t id, DebugResultCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }
  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(RemoveDebugPointOnProperThread(id));
    return;
  }
  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::RemoveDebugPointOnProperThread,
                     base::RetainedRef(this), id),
      std::move(callback));
}

void EmulatorImpl::SetDebugStackTraceEnabled(bool enabled,
                                             DebugResultCallback callback) {
  if (!is_power_on_) {
    std::move(callback).Run(false);
    return;
  }
  if (RunsTasksInCurrentSequence()) {
    std::move(callback).Run(SetDebugStackTraceEnabledOnProperThread(enabled));
    return;
  }
  emulator_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::SetDebugStackTraceEnabledOnProperThread,
                     base::RetainedRef(this), enabled),
      std::move(callback));
}

void EmulatorImpl::SetDebugBreakCallback(
    base::RepeatingClosure on_debug_break) {
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner =
      base::SingleThreadTaskRunner::HasCurrentDefault()
          ? base::SingleThreadTaskRunner::GetCurrentDefault()
          : nullptr;
  if (!emulator_task_runner_ || RunsTasksInCurrentSequence()) {
    debug_break_callback_ = std::move(on_debug_break);
    debug_break_task_runner_ = std::move(callback_task_runner);
    return;
  }
  emulator_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](scoped_refptr<EmulatorImpl> emulator,
                        base::RepeatingClosure callback,
                        scoped_refptr<base::SequencedTaskRunner> task_runner) {
                       emulator->debug_break_callback_ = std::move(callback);
                       emulator->debug_break_task_runner_ =
                           std::move(task_runner);
                     },
                     base::RetainedRef(this), std::move(on_debug_break),
                     std::move(callback_task_runner)));
}

Emulator::DebugMetrics EmulatorImpl::GetDebugMetrics() const {
  return DebugMetrics{debug_frame_count_.load(),
                      debug_last_frame_duration_us_.load(),
                      debug_last_audio_sample_count_.load()};
}

bool EmulatorImpl::LoadFromFileOnProperThread(const base::FilePath& rom_path,
                                              const LoadOptions& options) {
  DCHECK(RunsTasksInCurrentSequence());
  VFile* rom = VFileOpen(rom_path.AsUTF8Unsafe().c_str(), O_RDONLY);
  if (!rom)
    return false;
  return LoadROMOnProperThread(rom, options, std::nullopt);
}

bool EmulatorImpl::LoadFromBinaryOnProperThread(
    const Bytes& data,
    const LoadOptions& options,
    const std::optional<Bytes>& initial_save_data) {
  DCHECK(RunsTasksInCurrentSequence());
  if (data.empty())
    return false;

  VFile* rom = VFileMemChunk(data.data(), data.size());
  if (!rom)
    return false;
  return LoadROMOnProperThread(rom, options, initial_save_data);
}

bool EmulatorImpl::LoadROMOnProperThread(
    VFile* rom,
    const LoadOptions& options,
    const std::optional<Bytes>& initial_save_data) {
  DCHECK(RunsTasksInCurrentSequence());
  DCHECK(core_);
  DCHECK(rom);

  if (initial_save_data) {
    SaveType type = options.save_type == SaveType::kAuto
                        ? InferSaveType(initial_save_data->size())
                        : options.save_type;
    if (type == SaveType::kAuto || type == SaveType::kNone ||
        initial_save_data->size() != GetSaveTypeSize(type)) {
      rom->close(rom);
      return false;
    }
  }

  if (!core_->isROM(rom)) {
    rom->close(rom);
    return false;
  }

  ClearDebugPointsOnProperThread();
  last_debug_break_ = {};
  running_state_ = RunningState::kStopped;
  if (io_devices_ && io_devices_->rumble_device())
    io_devices_->rumble_device()->SetRumble(false);
  if (!core_->loadROM(core_, rom)) {
    rom->close(rom);
    is_rom_loaded_ = false;
    rom_data_.reset();
    ResetSaveDataTrackingOnProperThread();
    return false;
  }

  is_rom_loaded_ = true;
  core_->opts.skipBios = options.skip_bios;
  core_->opts.useBios = !options.skip_bios;
  ClearFrameBuffersOnProperThread();
  core_->reset(core_);

  if (options.save_type != SaveType::kAuto) {
    auto* gba = static_cast<GBA*>(core_->board);
    GBASavedataForceType(&gba->memory.savedata, ToMGBA(options.save_type));
  }

  PopulateRomDataOnProperThread();
  ResetSaveDataTrackingOnProperThread();
  if (initial_save_data && !ImportSaveDataOnProperThread(*initial_save_data)) {
    UnloadOnProperThread();
    return false;
  }
  UpdateSaveDataMetadataOnProperThread();
  return true;
}

bool EmulatorImpl::LoadBIOSFromFileOnProperThread(
    const base::FilePath& bios_path) {
  DCHECK(RunsTasksInCurrentSequence());
  VFile* bios = VFileOpen(bios_path.AsUTF8Unsafe().c_str(), O_RDONLY);
  if (!bios)
    return false;
  return LoadBIOSOnProperThread(bios);
}

bool EmulatorImpl::LoadBIOSFromBinaryOnProperThread(const Bytes& data) {
  DCHECK(RunsTasksInCurrentSequence());
  if (data.empty())
    return false;

  VFile* bios = VFileMemChunk(data.data(), data.size());
  if (!bios)
    return false;
  return LoadBIOSOnProperThread(bios);
}

bool EmulatorImpl::LoadBIOSOnProperThread(VFile* bios) {
  DCHECK(RunsTasksInCurrentSequence());
  DCHECK(core_);
  DCHECK(bios);

  if (bios->size(bios) != SIZE_BIOS || !core_->loadBIOS(core_, bios, 0)) {
    bios->close(bios);
    return false;
  }
  has_bios_ = true;
  return true;
}

bool EmulatorImpl::ImportSaveDataOnProperThread(const Bytes& data) {
  DCHECK(RunsTasksInCurrentSequence());
  DCHECK(core_);
  DCHECK(is_rom_loaded_);

  auto* gba = static_cast<GBA*>(core_->board);
  if (gba->memory.savedata.type == SAVEDATA_AUTODETECT) {
    SaveType inferred_type = InferSaveType(data.size());
    if (inferred_type == SaveType::kAuto)
      return false;
    GBASavedataForceType(&gba->memory.savedata, ToMGBA(inferred_type));
  }

  if (data.size() != GBASavedataSize(&gba->memory.savedata) ||
      !core_->savedataRestore(core_, data.data(), data.size(), true)) {
    return false;
  }

  observed_save_data_ = data;
  save_data_generation_ = 0;
  persisted_save_data_generation_ = 0;
  UpdateSaveDataMetadataOnProperThread();
  return true;
}

std::optional<Emulator::SaveDataSnapshot>
EmulatorImpl::ExportSaveDataOnProperThread(bool dirty_only) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !rom_data_ || !is_rom_loaded_)
    return std::nullopt;

  void* raw_data = nullptr;
  size_t size = core_->savedataClone(core_, &raw_data);
  if (!size || !raw_data) {
    std::free(raw_data);
    UpdateSaveDataMetadataOnProperThread();
    return std::nullopt;
  }

  Byte* begin = static_cast<Byte*>(raw_data);
  Bytes data(begin, begin + size);
  std::free(raw_data);

  if (data != observed_save_data_) {
    observed_save_data_ = data;
    ++save_data_generation_;
  }

  UpdateSaveDataMetadataOnProperThread();
  if (dirty_only && save_data_generation_ == persisted_save_data_generation_) {
    return std::nullopt;
  }

  return SaveDataSnapshot{rom_data_->crc32, std::move(data),
                          save_data_generation_};
}

bool EmulatorImpl::AcknowledgeSaveDataSavedOnProperThread(uint32_t rom_crc32,
                                                          uint64_t generation) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!rom_data_ || rom_data_->crc32 != rom_crc32 ||
      generation > save_data_generation_) {
    return false;
  }

  if (generation > persisted_save_data_generation_)
    persisted_save_data_generation_ = generation;
  return true;
}

void EmulatorImpl::PopulateRomDataOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  DCHECK(core_);
  DCHECK(is_rom_loaded_);

  char title[13] = {};
  char game_code[9] = {};
  core_->getGameTitle(core_, title);
  core_->getGameCode(core_, game_code);

  rom_data_ = std::make_unique<RomData>();
  rom_data_->title = TrimROMString(title, 12);
  rom_data_->game_code = TrimROMString(game_code, 8);
  rom_data_->rom_size = core_->romSize(core_);
  core_->checksum(core_, &rom_data_->crc32, mCHECKSUM_CRC32);
  UpdateSaveDataMetadataOnProperThread();
}

void EmulatorImpl::UpdateSaveDataMetadataOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !rom_data_ || !is_rom_loaded_)
    return;

  auto* gba = static_cast<GBA*>(core_->board);
  rom_data_->save_type = FromMGBA(gba->memory.savedata.type);
  rom_data_->save_size = GBASavedataSize(&gba->memory.savedata);
}

void EmulatorImpl::ResetSaveDataTrackingOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  observed_save_data_.clear();
  save_data_generation_ = 0;
  persisted_save_data_generation_ = 0;

  if (!core_ || !is_rom_loaded_)
    return;

  void* raw_data = nullptr;
  size_t size = core_->savedataClone(core_, &raw_data);
  if (size && raw_data) {
    Byte* begin = static_cast<Byte*>(raw_data);
    observed_save_data_.assign(begin, begin + size);
  }
  std::free(raw_data);
}

void EmulatorImpl::UnloadOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (io_devices_ && io_devices_->rumble_device())
    io_devices_->rumble_device()->SetRumble(false);
  ClearDebugPointsOnProperThread();
  if (core_ && is_rom_loaded_)
    core_->unloadROM(core_);

  is_rom_loaded_ = false;
  running_state_ = RunningState::kStopped;
  last_debug_break_ = {};
  rom_data_.reset();
  ResetSaveDataTrackingOnProperThread();
  ClearFrameBuffersOnProperThread();
}

void EmulatorImpl::ResetOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (core_ && is_rom_loaded_) {
    if (io_devices_ && io_devices_->rumble_device())
      io_devices_->rumble_device()->SetRumble(false);
    core_->reset(core_);
    last_debug_break_ = {};
    if (core_debugger_)
      mStackTraceClear(&core_debugger_->debugger.stackTrace);
    UpdateSaveDataMetadataOnProperThread();
  }
}

void EmulatorImpl::RunOneFrameOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_ || running_state_ != RunningState::kRunning) {
    return;
  }

  const auto started = std::chrono::steady_clock::now();
  UpdateInputOnProperThread();
  const uint32_t frame = core_->frameCounter(core_);
  mDebuggerPlatform* platform =
      core_debugger_ ? core_debugger_->debugger.platform : nullptr;
  if (platform && platform->hasBreakpoints(platform)) {
    while (core_->frameCounter(core_) == frame &&
           running_state_ == RunningState::kRunning) {
      core_->step(core_);
      if (core_debugger_->debugger.state == DEBUGGER_PAUSED)
        break;
      platform->checkBreakpoints(platform);
    }
  } else {
    core_->runFrame(core_);
  }

  if (core_->frameCounter(core_) != frame) {
    PresentFrameOnProperThread();
    DrainAudioOnProperThread();
    debug_frame_count_ = core_->frameCounter(core_);
    debug_last_frame_duration_us_ = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
  }
}

void EmulatorImpl::StepOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_)
    return;

  UpdateInputOnProperThread();
  if (core_debugger_)
    core_debugger_->debugger.state = DEBUGGER_RUNNING;
  core_->step(core_);
  mDebuggerPlatform* platform =
      core_debugger_ ? core_debugger_->debugger.platform : nullptr;
  if (platform && platform->hasBreakpoints(platform) &&
      core_debugger_->debugger.state != DEBUGGER_PAUSED) {
    platform->checkBreakpoints(platform);
  }
}

std::optional<Emulator::DebugSnapshot>
EmulatorImpl::GetDebugSnapshotOnProperThread(size_t instruction_count) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_ || running_state_ == RunningState::kRunning) {
    return std::nullopt;
  }

  auto* cpu = static_cast<ARMCore*>(core_->cpu);
  DebugSnapshot snapshot;
  for (size_t i = 0; i < snapshot.registers.size(); ++i)
    snapshot.registers[i] = static_cast<uint32_t>(cpu->gprs[i]);
  snapshot.cpsr = cpu->cpsr.packed;
  snapshot.spsr = cpu->spsr.packed;
  snapshot.cycles = mTimingGlobalTime(core_->timing);

  const bool is_thumb = cpu->executionMode == MODE_THUMB;
  snapshot.execution_mode =
      is_thumb ? DebugExecutionMode::kThumb : DebugExecutionMode::kArm;
  const uint32_t instruction_width = is_thumb ? WORD_SIZE_THUMB : WORD_SIZE_ARM;
  uint32_t address = snapshot.registers[ARM_PC] - instruction_width;
  address &= ~(instruction_width - 1);
  snapshot.instruction_address = address;

  instruction_count = std::min(instruction_count, kMaxDebugInstructionCount);
  snapshot.instructions.reserve(instruction_count);
  for (size_t i = 0; i < instruction_count; ++i) {
    DebugInstruction instruction;
    instruction.address = address;
    char disassembly[128] = {};
    ARMInstructionInfo info = {};

    if (is_thumb) {
      const uint16_t opcode =
          static_cast<uint16_t>(core_->rawRead16(core_, address, -1));
      const uint16_t next_opcode = static_cast<uint16_t>(
          core_->rawRead16(core_, address + WORD_SIZE_THUMB, -1));
      ARMInstructionInfo next_info = {};
      ARMInstructionInfo combined = {};
      ARMDecodeThumb(opcode, &info);
      ARMDecodeThumb(next_opcode, &next_info);
      if (ARMDecodeThumbCombine(&info, &next_info, &combined)) {
        instruction.opcode =
            (static_cast<uint32_t>(opcode) << 16) | next_opcode;
        instruction.width = WORD_SIZE_THUMB * 2;
        ARMDisassemble(&combined, cpu, core_->symbolTable,
                       address + WORD_SIZE_THUMB * 2, disassembly,
                       sizeof(disassembly));
      } else {
        instruction.opcode = opcode;
        instruction.width = WORD_SIZE_THUMB;
        ARMDisassemble(&info, cpu, core_->symbolTable,
                       address + WORD_SIZE_THUMB * 2, disassembly,
                       sizeof(disassembly));
      }
    } else {
      instruction.opcode = core_->rawRead32(core_, address, -1);
      instruction.width = WORD_SIZE_ARM;
      ARMDecodeARM(instruction.opcode, &info);
      ARMDisassemble(&info, cpu, core_->symbolTable,
                     address + WORD_SIZE_ARM * 2, disassembly,
                     sizeof(disassembly));
    }

    instruction.text = disassembly;
    snapshot.instructions.push_back(std::move(instruction));
    const uint32_t next_address = address + snapshot.instructions.back().width;
    if (next_address < address)
      break;
    address = next_address;
  }
  PopulateDebugStateOnProperThread(&snapshot);
  return snapshot;
}

std::optional<Emulator::DebugSnapshot>
EmulatorImpl::StepAndGetDebugSnapshotOnProperThread(size_t instruction_count) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_ || running_state_ == RunningState::kRunning) {
    return std::nullopt;
  }

  StepOnProperThread();
  return GetDebugSnapshotOnProperThread(instruction_count);
}

std::optional<Emulator::DebugMemorySnapshot>
EmulatorImpl::GetDebugMemorySnapshotOnProperThread(uint32_t address,
                                                   size_t byte_count) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_ || running_state_ == RunningState::kRunning)
    return std::nullopt;

  DebugMemorySnapshot snapshot;
  snapshot.address = address & ~0xfu;
  byte_count =
      std::min(byte_count ? byte_count : size_t{256}, kMaxDebugMemoryBytes);
  snapshot.data.reserve(byte_count);
  for (size_t i = 0; i < byte_count; ++i) {
    snapshot.data.push_back(static_cast<Byte>(core_->rawRead8(
        core_, snapshot.address + static_cast<uint32_t>(i), -1)));
  }

  const mCoreMemoryBlock* blocks = nullptr;
  const size_t block_count = core_->listMemoryBlocks(core_, &blocks);
  for (size_t i = 0; i < block_count; ++i) {
    if (blocks[i].flags & mCORE_MEMORY_VIRTUAL)
      continue;
    DebugMemoryRegion region;
    region.name = blocks[i].longName ? blocks[i].longName : blocks[i].shortName;
    region.start = blocks[i].start;
    region.end = blocks[i].end;
    region.writable = blocks[i].flags & mCORE_MEMORY_WRITE;
    snapshot.regions.push_back(std::move(region));
  }

  auto* gba = static_cast<GBA*>(core_->board);
  for (uint32_t offset = 0; offset < REG_MAX; offset += 2) {
    const char* name = GBAIORegisterNames[offset >> 1];
    if (!name)
      continue;
    snapshot.io_registers.push_back(
        DebugIORegister{name, BASE_IO + offset, gba->memory.io[offset >> 1]});
  }
  return snapshot;
}

std::optional<Emulator::DebugGraphicsSnapshot>
EmulatorImpl::GetDebugGraphicsSnapshotOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_ || running_state_ == RunningState::kRunning)
    return std::nullopt;

  auto* gba = static_cast<GBA*>(core_->board);
  DebugGraphicsSnapshot snapshot;
  std::copy(std::begin(gba->video.palette), std::end(gba->video.palette),
            snapshot.palette.begin());
  const Byte* vram = reinterpret_cast<const Byte*>(gba->video.vram);
  snapshot.vram.assign(vram, vram + SIZE_VRAM);
  snapshot.display_control = gba->memory.io[REG_DISPCNT >> 1];

  snapshot.sprites.reserve(128);
  for (size_t i = 0; i < 128; ++i) {
    const GBAObj& object = gba->video.oam.obj[i];
    DebugSprite sprite;
    sprite.index = static_cast<uint16_t>(i);
    sprite.transformed = GBAObjAttributesAIsTransformed(object.a);
    sprite.double_size =
        sprite.transformed && GBAObjAttributesAIsDoubleSize(object.a);
    sprite.enabled =
        sprite.transformed || !GBAObjAttributesAIsDisable(object.a);
    sprite.color_256 = GBAObjAttributesAIs256Color(object.a);
    sprite.x = static_cast<int16_t>(GBAObjAttributesBGetX(object.b));
    if (sprite.x >= 256)
      sprite.x -= 512;
    sprite.y = static_cast<int16_t>(GBAObjAttributesAGetY(object.a));
    if (sprite.y >= 160)
      sprite.y -= 256;
    const size_t size_index = GBAObjAttributesAGetShape(object.a) * 4 +
                              GBAObjAttributesBGetSize(object.b);
    sprite.width = static_cast<uint16_t>(GBAVideoObjSizes[size_index][0]);
    sprite.height = static_cast<uint16_t>(GBAVideoObjSizes[size_index][1]);
    sprite.tile = GBAObjAttributesCGetTile(object.c);
    sprite.palette = GBAObjAttributesCGetPalette(object.c);
    sprite.priority = GBAObjAttributesCGetPriority(object.c);
    if (!sprite.transformed) {
      sprite.hflip = GBAObjAttributesBIsHFlip(object.b);
      sprite.vflip = GBAObjAttributesBIsVFlip(object.b);
    }
    snapshot.sprites.push_back(sprite);
  }

  const mCoreChannelInfo* layers = nullptr;
  const size_t layer_count = core_->listVideoLayers(core_, &layers);
  for (size_t i = 0; i < layer_count; ++i) {
    snapshot.layers.push_back(
        DebugChannel{layers[i].id, layers[i].visibleName,
                     layers[i].visibleType ? layers[i].visibleType : "",
                     layers[i].id < debug_video_layers_enabled_.size()
                         ? debug_video_layers_enabled_[layers[i].id]
                         : true});
  }
  return snapshot;
}

std::optional<Emulator::DebugAudioSnapshot>
EmulatorImpl::GetDebugAudioSnapshotOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_)
    return std::nullopt;

  DebugAudioSnapshot snapshot;
  const mCoreChannelInfo* channels = nullptr;
  const size_t channel_count = core_->listAudioChannels(core_, &channels);
  for (size_t i = 0; i < channel_count; ++i) {
    snapshot.channels.push_back(
        DebugChannel{channels[i].id, channels[i].visibleName,
                     channels[i].visibleType ? channels[i].visibleType : "",
                     channels[i].id < debug_audio_channels_enabled_.size()
                         ? debug_audio_channels_enabled_[channels[i].id]
                         : true});
  }
  return snapshot;
}

bool EmulatorImpl::SetDebugVideoLayerEnabledOnProperThread(size_t id,
                                                           bool enabled) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || id >= debug_video_layers_enabled_.size())
    return false;
  core_->enableVideoLayer(core_, id, enabled);
  debug_video_layers_enabled_[id] = enabled;
  return true;
}

bool EmulatorImpl::SetDebugAudioChannelEnabledOnProperThread(size_t id,
                                                             bool enabled) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || id >= debug_audio_channels_enabled_.size())
    return false;
  core_->enableAudioChannel(core_, id, enabled);
  debug_audio_channels_enabled_[id] = enabled;
  return true;
}

bool EmulatorImpl::AddDebugBreakpointOnProperThread(uint32_t address) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!EnsureDebuggerOnProperThread())
    return false;
  mDebuggerPlatform* platform = core_debugger_->debugger.platform;
  mBreakpointList existing;
  mBreakpointListInit(&existing, 0);
  platform->listBreakpoints(platform, &existing);
  for (size_t i = 0; i < mBreakpointListSize(&existing); ++i) {
    if (mBreakpointListGetConstPointer(&existing, i)->address ==
        (address & ~1u)) {
      mBreakpointListDeinit(&existing);
      return false;
    }
  }
  mBreakpointListDeinit(&existing);
  mBreakpoint breakpoint = {};
  breakpoint.address = address;
  breakpoint.segment = -1;
  breakpoint.type = BREAKPOINT_HARDWARE;
  return platform->setBreakpoint(platform, &breakpoint) > 0;
}

bool EmulatorImpl::AddDebugWatchpointOnProperThread(uint32_t address,
                                                    DebugWatchpointType type) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!EnsureDebuggerOnProperThread())
    return false;
  mDebuggerPlatform* platform = core_debugger_->debugger.platform;
  mWatchpointList existing;
  mWatchpointListInit(&existing, 0);
  platform->listWatchpoints(platform, &existing);
  for (size_t i = 0; i < mWatchpointListSize(&existing); ++i) {
    if (mWatchpointListGetConstPointer(&existing, i)->address == address) {
      mWatchpointListDeinit(&existing);
      return false;
    }
  }
  mWatchpointListDeinit(&existing);
  mWatchpoint watchpoint = {};
  watchpoint.address = address;
  watchpoint.segment = -1;
  watchpoint.type = ToMGBAWatchpointType(type);
  return platform->setWatchpoint(platform, &watchpoint) > 0;
}

bool EmulatorImpl::RemoveDebugPointOnProperThread(int64_t id) {
  DCHECK(RunsTasksInCurrentSequence());
  return core_debugger_ && core_debugger_->debugger.platform &&
         core_debugger_->debugger.platform->clearBreakpoint(
             core_debugger_->debugger.platform, id);
}

bool EmulatorImpl::SetDebugStackTraceEnabledOnProperThread(bool enabled) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!enabled && !core_debugger_)
    return true;
  if (!EnsureDebuggerOnProperThread())
    return false;
  mDebuggerPlatform* platform = core_debugger_->debugger.platform;
  platform->setStackTraceMode(
      platform, enabled ? STACK_TRACE_ENABLED : STACK_TRACE_DISABLED);
  return true;
}

bool EmulatorImpl::EnsureDebuggerOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (core_debugger_)
    return core_debugger_->debugger.platform != nullptr;
  if (!core_)
    return false;
  core_debugger_ = std::make_unique<CoreDebugger>(this);
  mDebuggerAttach(&core_debugger_->debugger, core_);
  return core_debugger_->debugger.platform != nullptr;
}

void EmulatorImpl::PopulateDebugStateOnProperThread(DebugSnapshot* snapshot) {
  DCHECK(RunsTasksInCurrentSequence());
  DCHECK(snapshot);
  if (!core_debugger_ || !core_debugger_->debugger.platform)
    return;

  mDebuggerPlatform* platform = core_debugger_->debugger.platform;
  mBreakpointList breakpoints;
  mBreakpointListInit(&breakpoints, 0);
  platform->listBreakpoints(platform, &breakpoints);
  for (size_t i = 0; i < mBreakpointListSize(&breakpoints); ++i) {
    const mBreakpoint* point = mBreakpointListGetConstPointer(&breakpoints, i);
    snapshot->points.push_back(DebugPoint{point->id, point->address,
                                          DebugPointType::kBreakpoint,
                                          DebugWatchpointType::kWrite});
  }
  mBreakpointListDeinit(&breakpoints);

  mWatchpointList watchpoints;
  mWatchpointListInit(&watchpoints, 0);
  platform->listWatchpoints(platform, &watchpoints);
  for (size_t i = 0; i < mWatchpointListSize(&watchpoints); ++i) {
    const mWatchpoint* point = mWatchpointListGetConstPointer(&watchpoints, i);
    snapshot->points.push_back(DebugPoint{point->id, point->address,
                                          DebugPointType::kWatchpoint,
                                          FromMGBAWatchpointType(point->type)});
  }
  mWatchpointListDeinit(&watchpoints);

  snapshot->stack_trace_enabled =
      platform->getStackTraceMode(platform) != STACK_TRACE_DISABLED;
  const size_t depth =
      mStackTraceGetDepth(&core_debugger_->debugger.stackTrace);
  for (size_t i = 0; i < depth; ++i) {
    const mStackFrame* frame =
        mStackTraceGetFrame(&core_debugger_->debugger.stackTrace, i);
    if (!frame)
      continue;
    snapshot->stack_frames.push_back(
        DebugStackFrame{frame->callAddress, frame->entryAddress,
                        frame->frameBaseAddress, frame->interrupt});
  }
  snapshot->last_break = last_debug_break_;
}

void EmulatorImpl::ClearDebugPointsOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_debugger_ || !core_debugger_->debugger.platform)
    return;

  mDebuggerPlatform* platform = core_debugger_->debugger.platform;
  std::vector<ssize_t> ids;
  mBreakpointList breakpoints;
  mBreakpointListInit(&breakpoints, 0);
  platform->listBreakpoints(platform, &breakpoints);
  for (size_t i = 0; i < mBreakpointListSize(&breakpoints); ++i)
    ids.push_back(mBreakpointListGetConstPointer(&breakpoints, i)->id);
  mBreakpointListDeinit(&breakpoints);

  mWatchpointList watchpoints;
  mWatchpointListInit(&watchpoints, 0);
  platform->listWatchpoints(platform, &watchpoints);
  for (size_t i = 0; i < mWatchpointListSize(&watchpoints); ++i)
    ids.push_back(mWatchpointListGetConstPointer(&watchpoints, i)->id);
  mWatchpointListDeinit(&watchpoints);

  for (ssize_t id : ids)
    platform->clearBreakpoint(platform, id);
  mStackTraceClear(&core_debugger_->debugger.stackTrace);
}

void EmulatorImpl::OnDebuggerEnteredOnProperThread(int reason,
                                                   const void* entry_info) {
  DCHECK(RunsTasksInCurrentSequence());
  const auto* info = static_cast<const mDebuggerEntryInfo*>(entry_info);
  last_debug_break_ = {};
  last_debug_break_.address = info ? info->address : 0;
  last_debug_break_.point_id = info ? info->pointId : 0;
  switch (reason) {
    case DEBUGGER_ENTER_BREAKPOINT:
      last_debug_break_.reason = DebugBreakReason::kBreakpoint;
      break;
    case DEBUGGER_ENTER_WATCHPOINT:
      last_debug_break_.reason = DebugBreakReason::kWatchpoint;
      if (info) {
        last_debug_break_.old_value = info->type.wp.oldValue;
        last_debug_break_.new_value = info->type.wp.newValue;
      }
      break;
    case DEBUGGER_ENTER_ILLEGAL_OP:
      last_debug_break_.reason = DebugBreakReason::kIllegalInstruction;
      break;
    case DEBUGGER_ENTER_STACK:
      last_debug_break_.reason = DebugBreakReason::kStack;
      break;
    case DEBUGGER_ENTER_MANUAL:
    case DEBUGGER_ENTER_ATTACHED:
      break;
  }
  running_state_ = RunningState::kPaused;

  if (debug_break_callback_ && debug_break_task_runner_) {
    debug_break_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce([](base::RepeatingClosure callback) { callback.Run(); },
                       debug_break_callback_));
  }
}

void EmulatorImpl::PowerOffOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  running_state_ = RunningState::kStopped;

  if (io_devices_ && io_devices_->rumble_device())
    io_devices_->rumble_device()->SetRumble(false);

  if (core_) {
    if (core_debugger_) {
      core_->detachDebugger(core_);
      core_debugger_.reset();
    }
    if (is_rom_loaded_)
      core_->unloadROM(core_);
    is_rom_loaded_ = false;

    if (config_initialized_) {
      mCoreConfigDeinit(&core_->config);
      config_initialized_ = false;
    }
    core_->deinit(core_);
    core_ = nullptr;
  }

  core_adapters_.reset();
  rom_data_.reset();
  ResetSaveDataTrackingOnProperThread();
  last_debug_break_ = {};
  debug_break_callback_.Reset();
  debug_break_task_runner_.reset();
  debug_frame_count_ = 0;
  debug_last_frame_duration_us_ = 0;
  debug_last_audio_sample_count_ = 0;
  has_bios_ = false;
  is_power_on_ = false;
  emulator_task_runner_.reset();
}

Bytes EmulatorImpl::SaveStateOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_)
    return {};

  VFile* state_file = VFileMemChunk(nullptr, 0);
  if (!state_file)
    return {};

  constexpr int kSaveStateFlags = SAVESTATE_SAVEDATA | SAVESTATE_RTC;
  if (!mCoreSaveStateNamed(core_, state_file, kSaveStateFlags)) {
    state_file->close(state_file);
    return {};
  }

  ssize_t size = state_file->size(state_file);
  if (size <= 0 || state_file->seek(state_file, 0, SEEK_SET) < 0) {
    state_file->close(state_file);
    return {};
  }

  Bytes state(static_cast<size_t>(size));
  if (state_file->read(state_file, state.data(), state.size()) != size)
    state.clear();
  state_file->close(state_file);
  return state;
}

bool EmulatorImpl::LoadStateOnProperThread(const Bytes& data) {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_ || data.size() < core_->stateSize(core_))
    return false;

  VFile* state_file = VFileMemChunk(data.data(), data.size());
  if (!state_file)
    return false;

  constexpr int kSaveStateFlags = SAVESTATE_SAVEDATA | SAVESTATE_RTC;
  bool success = mCoreLoadStateNamed(core_, state_file, kSaveStateFlags);
  state_file->close(state_file);
  if (success) {
    if (core_debugger_)
      mStackTraceClear(&core_debugger_->debugger.stackTrace);
    last_debug_break_ = {};
    UpdateSaveDataMetadataOnProperThread();
  }
  return success;
}

void EmulatorImpl::UpdateInputOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  DCHECK(core_);

  uint32_t keys = 0;
  if (io_devices_ && io_devices_->input_device()) {
    for (int button = 0; button < static_cast<int>(ControllerButton::kMax);
         ++button) {
      ControllerButton value = static_cast<ControllerButton>(button);
      if (io_devices_->input_device()->IsKeyDown(0, value))
        keys |= ToMGBAKeyMask(value);
    }
  }
  core_->setKeys(core_, keys);
}

void EmulatorImpl::PresentFrameOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  DCHECK(core_);

  Colors& completed_frame = frame_buffers_[current_frame_index_];
  for (Color& color : completed_frame)
    color = (color & kColorMask) | kOpaqueAlpha;

  last_frame_index_ = current_frame_index_;
  current_frame_index_ = (current_frame_index_ + 1) % frame_buffers_.size();
  core_->setVideoBuffer(core_, frame_buffers_[current_frame_index_].data(),
                        kFrameWidth);

  if (!io_devices_)
    return;

  FrameData frame;
  frame.frame_number = core_->frameCounter(core_);
  frame.pixels = &frame_buffers_[last_frame_index_];
  for (IODevices::RenderDevice* render_device : io_devices_->render_devices()) {
    CHECK(render_device);
    if (render_device->NeedRender())
      render_device->Render(frame);
  }
}

void EmulatorImpl::DrainAudioOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  DCHECK(core_);

  blip_t* left = core_->getAudioChannel(core_, 0);
  blip_t* right = core_->getAudioChannel(core_, 1);
  int available = std::min(blip_samples_avail(left), blip_samples_avail(right));
  if (available <= 0)
    return;

  audio_samples_.resize(static_cast<size_t>(available) * 2);
  int left_count =
      blip_read_samples(left, audio_samples_.data(), available, true);
  int right_count =
      blip_read_samples(right, audio_samples_.data() + 1, available, true);
  size_t sample_count =
      static_cast<size_t>(std::min(left_count, right_count)) * 2;
  audio_samples_.resize(sample_count);
  debug_last_audio_sample_count_ = static_cast<uint32_t>(sample_count);

  float volume = volume_;
  if (volume != 1.f) {
    for (Sample& sample : audio_samples_)
      sample = static_cast<Sample>(sample * volume);
  }

  if (io_devices_ && io_devices_->audio_device() && !audio_samples_.empty()) {
    io_devices_->audio_device()->OnSampleArrived(audio_samples_.data(),
                                                 audio_samples_.size());
  }
}

void EmulatorImpl::ClearFrameBuffersOnProperThread() {
  DCHECK(!emulator_task_runner_ || RunsTasksInCurrentSequence());
  for (Colors& frame : frame_buffers_)
    std::fill(frame.begin(), frame.end(), kOpaqueAlpha);
  current_frame_index_ = 0;
  last_frame_index_ = 1;
  if (core_) {
    core_->setVideoBuffer(core_, frame_buffers_[current_frame_index_].data(),
                          kFrameWidth);
  }
}

bool EmulatorImpl::RunsTasksInCurrentSequence() const {
  return emulator_task_runner_ &&
         emulator_task_runner_->RunsTasksInCurrentSequence();
}

}  // namespace gba
}  // namespace kiwi
