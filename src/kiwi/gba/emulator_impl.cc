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

#include "gba/emulator_impl.h"

#include <fcntl.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#undef LIKELY
#undef UNLIKELY

#include <mgba-util/vfs.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/input.h>
#include <mgba/internal/gba/savedata.h>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace kiwi {
namespace gba {
namespace {

constexpr size_t kAudioBufferSamples = 2048;
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

static_assert(static_cast<int>(ControllerButton::kA) == GBA_KEY_A);
static_assert(static_cast<int>(ControllerButton::kB) == GBA_KEY_B);
static_assert(static_cast<int>(ControllerButton::kSelect) == GBA_KEY_SELECT);
static_assert(static_cast<int>(ControllerButton::kStart) == GBA_KEY_START);
static_assert(static_cast<int>(ControllerButton::kRight) == GBA_KEY_RIGHT);
static_assert(static_cast<int>(ControllerButton::kLeft) == GBA_KEY_LEFT);
static_assert(static_cast<int>(ControllerButton::kUp) == GBA_KEY_UP);
static_assert(static_cast<int>(ControllerButton::kDown) == GBA_KEY_DOWN);
static_assert(static_cast<int>(ControllerButton::kR) == GBA_KEY_R);
static_assert(static_cast<int>(ControllerButton::kL) == GBA_KEY_L);
static_assert(static_cast<int>(ControllerButton::kMax) == GBA_KEY_MAX);

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
  if (core_ && is_rom_loaded_)
    core_->unloadROM(core_);

  is_rom_loaded_ = false;
  running_state_ = RunningState::kStopped;
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
    UpdateSaveDataMetadataOnProperThread();
  }
}

void EmulatorImpl::RunOneFrameOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_ || running_state_ != RunningState::kRunning) {
    return;
  }

  UpdateInputOnProperThread();
  core_->runFrame(core_);
  PresentFrameOnProperThread();
  DrainAudioOnProperThread();
}

void EmulatorImpl::StepOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  if (!core_ || !is_rom_loaded_)
    return;

  UpdateInputOnProperThread();
  core_->step(core_);
}

void EmulatorImpl::PowerOffOnProperThread() {
  DCHECK(RunsTasksInCurrentSequence());
  running_state_ = RunningState::kStopped;

  if (io_devices_ && io_devices_->rumble_device())
    io_devices_->rumble_device()->SetRumble(false);

  if (core_) {
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
  if (success)
    UpdateSaveDataMetadataOnProperThread();
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
      if (io_devices_->input_device()->IsKeyDown(value))
        keys |= 1u << button;
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
