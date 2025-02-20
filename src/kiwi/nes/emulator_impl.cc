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

#include "nes/emulator_impl.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <queue>

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "nes/cpu.h"
#include "nes/cpu_bus.h"
#include "nes/emulator.h"
#include "nes/emulator_states.h"
#include "nes/ppu.h"
#include "nes/ppu_bus.h"
#include "nes/registers.h"

namespace kiwi {
namespace nes {
constexpr std::chrono::nanoseconds kNanoPerCycle =
    std::chrono::nanoseconds(559);

namespace {
class EmulatorRenderTaskRunner : public base::SequencedTaskRunner {
 public:
  friend class base::RefCountedThreadSafe<EmulatorRenderTaskRunner>;
  EmulatorRenderTaskRunner() = default;

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

  bool PostTaskAndReply(const base::Location& from_here,
                        base::OnceClosure task,
                        base::OnceClosure reply) override;

  static EmulatorRenderTaskRunner* AsEmulatorRenderTaskRunner(
      scoped_refptr<base::SequencedTaskRunner> task_runner) {
    return static_cast<EmulatorRenderTaskRunner*>(task_runner.get());
  }

 private:
  ~EmulatorRenderTaskRunner() = default;

 public:
  void RunAllTasks();
  base::OnceClosure PopTask();
  bool HasTask();

 private:
  std::queue<base::OnceClosure> tasks_while_rendering_unsafe_;
  std::mutex mutex_for_tasks_while_rendering_;
};

bool EmulatorRenderTaskRunner::PostDelayedTask(const base::Location& from_here,
                                               base::OnceClosure task,
                                               base::TimeDelta delay) {
  if (delay != base::TimeDelta()) {
    LOG(ERROR) << "Post with delay is not supported.";
    return false;
  }

  std::lock_guard<std::mutex> guard(mutex_for_tasks_while_rendering_);
  tasks_while_rendering_unsafe_.push(std::move(task));

  return true;
}

bool EmulatorRenderTaskRunner::PostTaskAndReply(const base::Location& from_here,
                                                base::OnceClosure task,
                                                base::OnceClosure reply) {
  scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  base::OnceClosure c = std::move(task).Then(base::BindOnce(
      [](const base::Location& from_here,
         scoped_refptr<base::SingleThreadTaskRunner> reply_task_runner,
         base::OnceClosure reply) {
        reply_task_runner->PostTask(from_here, std::move(reply));
      },
      from_here, base::RetainedRef(reply_task_runner), std::move(reply)));
  return PostTask(from_here, std::move(c));
}

void EmulatorRenderTaskRunner::RunAllTasks() {
  // Checks if any task to be run
  while (HasTask()) {
    base::OnceClosure task = PopTask();
    std::move(task).Run();
  }
}

bool EmulatorRenderTaskRunner::HasTask() {
  std::lock_guard<std::mutex> guard(mutex_for_tasks_while_rendering_);
  return !tasks_while_rendering_unsafe_.empty();
}

base::OnceClosure EmulatorRenderTaskRunner::PopTask() {
  std::lock_guard<std::mutex> guard(mutex_for_tasks_while_rendering_);
  base::OnceClosure task = std::move(tasks_while_rendering_unsafe_.front());
  tasks_while_rendering_unsafe_.pop();
  return task;
}

}  // namespace

EmulatorImpl::EmulatorImpl()
    : controller1_(0),
      controller2_(1),
      render_coroutine_(base::MakeRefCounted<EmulatorRenderTaskRunner>()) {}

EmulatorImpl::~EmulatorImpl() = default;

void EmulatorImpl::PowerOn() {
  emulator_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();

  ppu_bus_ = std::make_unique<PPUBus>();
  ppu_ = std::make_unique<PPU>(ppu_bus_.get());
  ppu_->SetObserver(this);

  cpu_bus_ = std::make_unique<CPUBus>();
  cpu_bus_->set_ppu(ppu_.get());
  cpu_bus_->set_emulator(this);

  cpu_ = std::make_unique<CPU>(cpu_bus_.get());
  cpu_->SetObserver(this);

  // Set callback for NMI interrupt
  ppu_->set_cpu_nmi_callback(base::BindRepeating(
      &CPU::Interrupt, base::Unretained(cpu_.get()), CPU::InterruptType::NMI));

  // Power up CPU, initialize memory and registers.
  cpu_->PowerUp();

  apu_ = std::make_unique<APU>(this, cpu_bus_.get());
  apu_->SetIRQCallback(
      base::BindRepeating(&EmulatorImpl::OnIRQFromAPU, base::Unretained(this)));
  is_power_on_ = true;

  if (debug_port_) {
    debug_port_->OnCPUPowerOn(GetCPUContext());
    debug_port_->OnPPUPowerOn(GetPPUContext());
  }
}

void EmulatorImpl::PowerOff() {
  if (is_power_on()) {
    if (GetRunningState() == RunningState::kRunning)
      running_state_ = Emulator::RunningState::kStopped;

    if (emulator_task_runner_->RunsTasksInCurrentSequence()) {
      RunOneFrameOnProperThread();
      PowerOffOnProperThread();
    } else {
      emulator_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&EmulatorImpl::RunOneFrameOnProperThread,
                         base::RetainedRef(this))
              .Then(base::BindOnce(&EmulatorImpl::PowerOffOnProperThread,
                                   base::RetainedRef(this))));
    }
  }
}

void EmulatorImpl::LoadFromFile(const base::FilePath& rom_path,
                                LoadCallback callback) {
  render_coroutine_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::LoadFromFileOnProperThread,
                     base::RetainedRef(this), rom_path),
      base::BindOnce(std::move(callback)));
}

void EmulatorImpl::LoadFromBinary(const Bytes& data, LoadCallback callback) {
  render_coroutine_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::LoadFromBinaryOnProperThread,
                     base::RetainedRef(this), data),
      base::BindOnce(std::move(callback)));
}

const RomData* EmulatorImpl::GetRomData() {
  if (cartridge_)
    return cartridge_->GetRomData();

  return nullptr;
}

void EmulatorImpl::Run() {
  if (!cartridge_) {
    LOG(ERROR) << "ROM has not been loaded. Call Load() first.";
    return;
  }

  if (!cartridge_->is_loaded()) {
    LOG(ERROR) << "Failed to run, because of loading cartridge failure.";
    return;
  }

  running_state_ = RunningState::kRunning;
}

void EmulatorImpl::RunOneFrame() {
  if (emulator_task_runner_->RunsTasksInCurrentSequence()) {
    RunOneFrameOnProperThread();
  } else {
    emulator_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&EmulatorImpl::RunOneFrameOnProperThread,
                                  base::RetainedRef(this)));
  }
}

void EmulatorImpl::Pause() {
  // If running state is kStopped, the state should remain stopped.
  if (running_state_ == Emulator::RunningState::kRunning)
    running_state_ = RunningState::kPaused;
}

void EmulatorImpl::LoadAndRun(const base::FilePath& rom_path,
                              LoadCallback callback) {
  LoadCallback load_callback = base::BindOnce(
      [](scoped_refptr<EmulatorImpl> emulator, LoadCallback callback,
         bool success) {
        if (success) {
          emulator->Run();
        } else {
          LOG(ERROR) << "Error occurs when load ROM via " << __func__;
        }
        std::move(callback).Run(success);
      },
      base::RetainedRef(this), std::move(callback));
  LoadFromFile(rom_path, std::move(load_callback));
}

void EmulatorImpl::LoadAndRun(const Bytes& data, LoadCallback callback) {
  LoadCallback load_callback = base::BindOnce(
      [](scoped_refptr<EmulatorImpl> emulator, LoadCallback callback,
         bool success) {
        if (success) {
          emulator->Run();
        } else {
          LOG(ERROR) << "Error occurs when load ROM via " << __func__;
        }
        std::move(callback).Run(success);
      },
      base::RetainedRef(this), std::move(callback));
  LoadFromBinary(data, std::move(load_callback));
}

void EmulatorImpl::Unload(UnloadCallback callback) {
  CHECK(is_power_on()) << "Make sure Emulator is power on.";
  running_state_ = RunningState::kStopped;
  Reset(std::move(callback));
}

void EmulatorImpl::Reset(ResetCallback reset_callback) {
  RunningState last_state = running_state_;
  Pause();
  render_coroutine_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          [](EmulatorImpl* emulator, RunningState last_state) {
            emulator->ResetOnProperThread();
            emulator->PostReset(last_state);
          },
          base::RetainedRef(this), last_state),
      std::move(reset_callback));
}

void EmulatorImpl::DMA(Byte page) {
  cpu_->SkipDMACycles();
  Byte* page_ptr = cpu_bus_->GetPagePointer(page);
  if (page_ptr) {
    ppu_->DMA(page_ptr);
  } else {
    LOG(ERROR) << "Can't get page pointer for DMA.";
  }
}

void EmulatorImpl::Strobe(Byte strobe) {
  controller1_.Strobe(strobe);
  controller2_.Strobe(strobe);
}

void EmulatorImpl::RunOneFrameOnProperThread() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (running_state_ != RunningState::kRunning) {
    EmulatorRenderTaskRunner::AsEmulatorRenderTaskRunner(render_coroutine_)
        ->RunAllTasks();
    return;
  }

  if (debug_port_)
    debug_port_->performance_counter().Start();

  // A frame has about 29781 CPU loops.
  constexpr int kLoopsPerFrame = 29781;
  for (int loop = 0; loop < kLoopsPerFrame; ++loop) {
    if (running_state_ != RunningState::kRunning)
      break;
    StepInternal();
  }
  if (debug_port_)
    debug_port_->performance_counter().End();

  EmulatorRenderTaskRunner::AsEmulatorRenderTaskRunner(render_coroutine_)
      ->RunAllTasks();
}

void EmulatorImpl::PowerOffOnProperThread() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  SetDebugPort(nullptr);
  cpu_.reset();
  ppu_.reset();
  ppu_bus_.reset();
  cpu_bus_.reset();
  if (ppu_)
    ppu_->RemoveObserver();
  if (cpu_)
    cpu_->RemoveObserver();
}

Bytes EmulatorImpl::SaveStateOnProperThread() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());

  if (running_state_ != Emulator::RunningState::kStopped) {
    return EmulatorStates::CreateStateForVersion(this, 1).Build();
  } else {
    return Bytes();
  }
}

bool EmulatorImpl::LoadStateOnProperThread(const Bytes& data) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());

  bool success;
  if (running_state_ != Emulator::RunningState::kStopped) {
    success = EmulatorStates::CreateStateForVersion(this, 1).Restore(data);
  } else {
    success = false;
  }

  // Reset cpu cycle, for calculate next frame correctly.
  Run();
  return success;
}

void EmulatorImpl::ResetOnProperThread() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(cpu_ && ppu_);
  if (cartridge_ && cartridge_->is_loaded()) {
    cpu_->Reset();
    ppu_->Reset();
    apu_->Reset();
  }
}

void EmulatorImpl::UnloadOnProperThread() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  CHECK(is_power_on()) << "Make sure Emulator is power on.";
  running_state_ = RunningState::kStopped;
  ResetOnProperThread();
}

void EmulatorImpl::PostReset(RunningState last_state) {
  if (debug_port_) {
    debug_port_->OnCPUReset(GetCPUContext());
    debug_port_->OnPPUReset(GetPPUContext());
  }
  if (last_state == RunningState::kRunning) {
    Run();
  }
}

void EmulatorImpl::OnIRQFromAPU() {
  // TODO Handle APU IRQ
  DLOG(INFO) << "OnIRQFromAPU() is unhandled yet.";
}

void EmulatorImpl::SetControllerTypes(uint32_t crc32) {
  switch (crc32) {
    case 0x24598791:  // Duck Hunt
    case 0xB8B9ACA3:  // Wild Gunman (Japan, USA)
    case 0x5112dc21:  // Wild Gunman (World) (Rev A)
    case 0xff24d794:  // Hogan's Alley (World)
    case 0x3e58a87e:  // Freedom Force (USA)
    case 0xde8fd935:  // To the Earth (USA)
      controller1_.SetType(this, Controller::Type::kStandard);
      controller2_.SetType(this, Controller::Type::kZapper);
      break;
    default:
      controller1_.SetType(this, Controller::Type::kStandard);
      controller2_.SetType(this, Controller::Type::kStandard);
      break;
  }
}

void EmulatorImpl::OnPPUStepped() {
  if (debug_port_) {
    debug_port_->OnPPUStepped(GetPPUContext());
  }
}

void EmulatorImpl::Step() {
  DCHECK(running_state_ != RunningState::kRunning)
      << "Step() should be called when emulator is paused.";
  StepInternal();
}

bool EmulatorImpl::LoadFromFileOnProperThread(const base::FilePath& rom_path) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  scoped_refptr<Cartridge> cartridge = base::MakeRefCounted<Cartridge>(this);
  return HandleLoadedResult(cartridge->Load(rom_path), cartridge);
}

bool EmulatorImpl::LoadFromBinaryOnProperThread(const Bytes& data) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  scoped_refptr<Cartridge> cartridge = base::MakeRefCounted<Cartridge>(this);
  return HandleLoadedResult(cartridge->Load(data), cartridge);
}

bool EmulatorImpl::HandleLoadedResult(Cartridge::LoadResult load_result,
                                      scoped_refptr<Cartridge> cartridge) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (!load_result.success)
    return false;

  UnloadOnProperThread();
  cartridge_ = cartridge;
  if (debug_port_) {
    debug_port_->OnRomLoaded(load_result.success, load_result.success
                                                      ? cartridge->GetRomData()
                                                      : nullptr);
  }

  // Set patch config for PPU
  ppu_->SetPatch(cartridge->crc32());

  SetControllerTypes(cartridge->crc32());

  // Set mapper for buses.
  cpu_bus_->SetMapper(cartridge->mapper());
  ppu_bus_->SetMapper(cartridge->mapper());
  cartridge->mapper()->set_mirroring_changed_callback(base::BindRepeating(
      &PPUBus::UpdateMirroring, base::Unretained(ppu_bus_.get())));
  cartridge->mapper()->set_scanline_irq_callback(base::BindRepeating(
      &CPU::Interrupt, base::Unretained(cpu_.get()), CPU::InterruptType::IRQ));

  // Reset CPU and PPU.
  ResetOnProperThread();
  return true;
}

void EmulatorImpl::StepInternal() {
  apu_->increase_cycles();

  // https://www.nesdev.org/wiki/Cycle_reference_chart
  // PPU
  if (debug_port_)
    debug_port_->performance_counter().PPUStart();
  ppu_->Step();
  ppu_->Step();
  ppu_->Step();
  if (debug_port_)
    debug_port_->performance_counter().PPUEnd();

  // CPU
  if (debug_port_)
    debug_port_->performance_counter().CPUStart();
  cpu_->Step();
  if (debug_port_)
    debug_port_->performance_counter().CPUEnd();

  if (debug_port_)
    debug_port_->OnEmulatorStepped(GetCPUContext(), GetPPUContext());
}

void EmulatorImpl::SetDebugPort(DebugPort* debug_port) {
  debug_port_ = debug_port;
}

Emulator::RunningState EmulatorImpl::GetRunningState() {
  return running_state_;
}

void EmulatorImpl::SetIODevices(std::unique_ptr<IODevices> io_devices) {
  io_devices_ = std::move(io_devices);
}

IODevices* EmulatorImpl::GetIODevices() {
  return io_devices_.get();
}

void EmulatorImpl::SaveState(SaveStateCallback callback) {
  render_coroutine_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::SaveStateOnProperThread,
                     base::RetainedRef(this)),
      std::move(callback));
}

void EmulatorImpl::LoadState(const Bytes& data, LoadCallback callback) {
  render_coroutine_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&EmulatorImpl::LoadStateOnProperThread,
                     base::RetainedRef(this), data),
      std::move(callback));
}

void EmulatorImpl::SetVolume(float volume) {
  apu_->SetVolume(volume);
}

float EmulatorImpl::GetVolume() {
  return apu_->GetVolume();
}

const Colors& EmulatorImpl::GetLastFrame() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  return ppu_->last_frame();
}

Byte EmulatorImpl::Read(Address address) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  switch (static_cast<IORegister>(address)) {
    case IORegister::OAMDMA:
      return address & 0xff;
    case IORegister::JOY1:
      return controller1_.Read();
    case IORegister::JOY2:
      return controller2_.Read();
    default:
      break;
  }

  switch (static_cast<APURegister>(address)) {
    case APURegister::STATUS:
      return apu_->Read(address);
    default:
      LOG(WARNING) << "Address $" << Hex<16>{address}
                   << " is not handled for reading.";
      return 0;
  }
}

void EmulatorImpl::Write(Address address, Byte value) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  switch (static_cast<IORegister>(address)) {
    case IORegister::OAMDMA:
      return DMA(value);
    case IORegister::JOY1: {
      controller1_.Strobe(value);
      controller2_.Strobe(value);
      return;
    }
    default:
      break;
  }

  if (address >= static_cast<Address>(APURegister::PULSE1_1) &&
      address <= static_cast<Address>(APURegister::FRAME_COUNTER)) {
    apu_->Write(address, value);
  } else {
    DLOG(INFO) << "Address $" << Hex<16>{address}
               << " is not handled for write.";
  }
}

void EmulatorImpl::OnPPUADDR(Address address) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (debug_port_) {
    debug_port_->OnPPUADDR(address);
  }
}

void EmulatorImpl::OnPPUScanlineStart(int scanline) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (debug_port_) {
    debug_port_->OnScanlineStart(scanline);
  }
}

void EmulatorImpl::OnPPUScanlineEnd(int scanline) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (debug_port_) {
    debug_port_->OnScanlineEnd(scanline);
  }
}

void EmulatorImpl::OnPPUFrameStart() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (debug_port_) {
    debug_port_->OnFrameStart();
  }
}

void EmulatorImpl::OnPPUFrameEnd() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (debug_port_) {
    debug_port_->OnFrameEnd();
  }
}

void EmulatorImpl::OnRenderReady(const Colors& swapbuffer) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  // Render is ready, update APU state here.

  apu_->StepFrame();

  if (debug_port_) {
    if (debug_port_->render_paused())
      return;

    debug_port_->OnNametableRenderReady();
  }

  if (io_devices_) {
    for (IODevices::RenderDevice* render_device :
         io_devices_->render_devices()) {
      CHECK(render_device);
      if (render_device->NeedRender()) {
        render_device->Render(256, 240, swapbuffer);
      }
    }
  }
}

void EmulatorImpl::OnCPUNMI() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (debug_port_)
    debug_port_->OnCPUNMI();
}

void EmulatorImpl::OnCPUBeforeStep(CPUDebugState& state) {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (debug_port_)
    debug_port_->OnCPUBeforeStep(state);
}

void EmulatorImpl::OnCPUStepped() {
  DCHECK(emulator_task_runner_->RunsTasksInCurrentSequence());
  if (debug_port_)
    debug_port_->OnCPUStepped(GetCPUContext());
}

PPUContext EmulatorImpl::GetPPUContext() {
  DCHECK(ppu_);
  return PPUContext{ppu_->registers(),    ppu_->data_address(),
                    ppu_->write_toggle(), ppu_->sprite_data_address(),
                    ppu_->palette(),      ppu_->scanline(),
                    ppu_->pixel(),        ppu_->patch()};
}

CPUContext EmulatorImpl::GetCPUContext() {
  DCHECK(cpu_);
  return CPUContext{cpu_->registers(), cpu_->get_last_action()};
}

Byte EmulatorImpl::GetCPUMemory(Address address) {
  DCHECK(cpu_bus_);
  return cpu_bus_->Read(address);
}

Byte EmulatorImpl::GetPPUMemory(Address address) {
  DCHECK(ppu_bus_);
  return ppu_bus_->Read(address);
}

Byte EmulatorImpl::GetOAMMemory(Byte address) {
  DCHECK(ppu_);
  return ppu_->ReadOAMData(address);
}

const Colors& EmulatorImpl::GetCurrentFrame() {
  DCHECK(ppu_);
  return ppu_->current_frame();
}

void EmulatorImpl::SetAudioChannelMasks(int audio_channels) {
  DCHECK(apu_);
  apu_->SetAudioChannels(audio_channels);
}

int EmulatorImpl::GetAudioChannelMasks() {
  DCHECK(apu_);
  return apu_->GetAudioChannels();
}

Controller::Type EmulatorImpl::GetControllerType(int id) {
  DCHECK(id == 0 || id == 1);
  if (id == 0) {
    return controller1_.type();
  } else {
    return controller2_.type();
  }
}

void EmulatorImpl::SetControllerType(int id, Controller::Type type) {
  DCHECK(id == 0 || id == 1);
  if (id == 0) {
    return controller1_.SetType(this, type);
  } else {
    return controller2_.SetType(this, type);
  }
}

}  // namespace nes
}  // namespace kiwi
