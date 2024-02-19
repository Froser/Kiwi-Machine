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

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "nes/bus.h"
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

EmulatorImpl::EmulatorImpl(bool emulate_on_working_thread)
    : controller1_(0),
      controller2_(1),
      emulate_on_working_thread_(emulate_on_working_thread) {}

EmulatorImpl::~EmulatorImpl() = default;

void EmulatorImpl::PowerOn() {
  emulator_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  if (!working_thread_)
    working_thread_ = std::make_unique<base::Thread>("NES Working thread");
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  if (!working_thread_->StartWithOptions(std::move(options))) {
    LOG(FATAL) << "Failed to start working thread.";
  }
  io_task_runner_ = working_thread_->task_runner();
  working_task_runner_ = emulate_on_working_thread_
                             ? io_task_runner_
                             : base::SequencedTaskRunner::GetCurrentDefault();

  controller1_.set_emulator(this);
  controller2_.set_emulator(this);

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

    if (working_task_runner_->RunsTasksInCurrentSequence()) {
      RunOneFrameOnProperThread();
      PowerOffOnProperThread();
    } else {
      working_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&EmulatorImpl::RunOneFrameOnProperThread,
                         base::Unretained(this))
              .Then(base::BindOnce(&EmulatorImpl::PowerOffOnProperThread,
                                   base::RetainedRef(this))));
    }
  }
}

void EmulatorImpl::LoadFromFile(const base::FilePath& rom_path,
                                LoadCallback callback) {
  Pause();
  cartridge_ = base::MakeRefCounted<Cartridge>(this);
  cartridge_->Load(rom_path, MakeLoadCallback(std::move(callback)));
}

void EmulatorImpl::LoadFromBinary(const Bytes& data, LoadCallback callback) {
  Pause();
  cartridge_ = base::MakeRefCounted<Cartridge>(this);
  cartridge_->Load(data, MakeLoadCallback(std::move(callback)));
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

  cpu_cycle_timestamp_ = std::chrono::high_resolution_clock::now();
  running_state_ = RunningState::kRunning;
}

void EmulatorImpl::RunOneFrame() {
  if (working_task_runner_->RunsTasksInCurrentSequence()) {
    RunOneFrameOnProperThread();
  } else {
    working_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&EmulatorImpl::RunOneFrameOnProperThread,
                                  base::Unretained(this)));
  }
}

void EmulatorImpl::Pause() {
  // If running state is kStopped, the state should remain stopped.
  if (running_state_ == Emulator::RunningState::kRunning)
    running_state_ = RunningState::kPaused;
}

void EmulatorImpl::LoadAndRun(const base::FilePath& rom_path,
                              base::OnceClosure callback) {
  base::OnceClosure reply = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback));

  base::OnceClosure load_closure = base::BindOnce(
      &EmulatorImpl::LoadFromFile, base::RetainedRef(this), rom_path,
      base::IgnoreArgs<bool>(
          base::BindOnce(&EmulatorImpl::Run, base::RetainedRef(this)))
          .Then(std::move(reply)));

  if (working_task_runner_->RunsTasksInCurrentSequence()) {
    Unload(std::move(load_closure));
  } else {
    working_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&EmulatorImpl::Unload, base::Unretained(this),
                                  std::move(load_closure)));
  }
}

void EmulatorImpl::LoadAndRun(const Bytes& data, base::OnceClosure callback) {
  base::OnceClosure reply = base::BindPostTask(
      base::SequencedTaskRunner::GetCurrentDefault(), std::move(callback));

  base::OnceClosure load_closure = base::BindOnce(
      &EmulatorImpl::LoadFromBinary, base::Unretained(this), std::move(data),
      base::IgnoreArgs<bool>(
          base::BindOnce(&EmulatorImpl::Run, base::RetainedRef(this)))
          .Then(std::move(reply)));

  if (working_task_runner_->RunsTasksInCurrentSequence()) {
    Unload(std::move(load_closure));
  } else {
    working_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&EmulatorImpl::Unload, base::Unretained(this),
                                  std::move(load_closure)));
  }
}

void EmulatorImpl::Unload(UnloadCallback callback) {
  CHECK(is_power_on()) << "Make sure Emulator is power on.";
  running_state_ = RunningState::kStopped;
  // Post One more task, to make sure working task runner dealing with kPaused
  // running state.
  Reset(std::move(callback));
}

void EmulatorImpl::Reset(ResetCallback reset_callback) {
  RunningState last_state = running_state_;
  Pause();
  // Push a reset task to working thread, make sure it runs in the last
  // sequence.
  if (working_task_runner_->RunsTasksInCurrentSequence()) {
    ResetOnProperThread();
    AfterResetReply(last_state, std::move(reset_callback));
  } else {
    working_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&EmulatorImpl::ResetOnProperThread,
                       base::RetainedRef(this)),
        base::BindOnce(&EmulatorImpl::AfterResetReply, base::RetainedRef(this),
                       last_state, std::move(reset_callback)));
  }
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
  DCHECK(working_task_runner_->RunsTasksInCurrentSequence());
  if (running_state_ != RunningState::kRunning)
    return;

  cpu_cycle_elapsed_ +=
      (std::chrono::high_resolution_clock::now() - cpu_cycle_timestamp_);
  cpu_cycle_timestamp_ = std::chrono::high_resolution_clock::now();

  if (cpu_cycle_elapsed_ > kNanoPerCycle * 1000000) {
    // If we stuck too long, it might be in a debug mode. Just treats it as one
    // frame.
    cpu_cycle_elapsed_ = kNanoPerCycle + std::chrono::nanoseconds(1);
  }

  while (cpu_cycle_elapsed_ > kNanoPerCycle) {
    if (running_state_ != RunningState::kRunning)
      break;
    StepInternal();
    cpu_cycle_elapsed_ -= kNanoPerCycle;
  }
}

void EmulatorImpl::DoNextRunOnProperThread() {
  DCHECK(working_task_runner_->RunsTasksInCurrentSequence());
  if (running_state_ == RunningState::kRunning) {
    // Post the task again
    working_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&EmulatorImpl::RunOneFrameOnProperThread,
                                  base::RetainedRef(this)));
  }
}

void EmulatorImpl::PowerOffOnProperThread() {
  DCHECK(working_task_runner_->RunsTasksInCurrentSequence());
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
  DCHECK(working_task_runner_->RunsTasksInCurrentSequence());

  if (running_state_ != Emulator::RunningState::kStopped) {
    return EmulatorStates::CreateStateForVersion(this, 1).Build();
  } else {
    return Bytes();
  }
}

bool EmulatorImpl::LoadStateOnProperThread(const Bytes& data) {
  DCHECK(working_task_runner_->RunsTasksInCurrentSequence());

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
  DCHECK(working_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(cpu_ && ppu_);
  cpu_cycle_elapsed_ = std::chrono::nanoseconds(0);
  if (cartridge_ && cartridge_->is_loaded()) {
    cpu_->Reset();
    ppu_->Reset();
    apu_->Reset();
  }
}

void EmulatorImpl::AfterResetReply(RunningState last_state,
                                   ResetCallback callback) {
  if (debug_port_) {
    debug_port_->OnCPUReset(GetCPUContext());
    debug_port_->OnPPUReset(GetPPUContext());
  }
  if (last_state == RunningState::kRunning) {
    Run();
  }
  std::move(callback).Run();
}

void EmulatorImpl::OnIRQFromAPU() {
  // TODO Handle APU IRQ
  LOG(INFO) << "OnIRQFromAPU() is unhandled yet.";
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

Cartridge::LoadCallback EmulatorImpl::MakeLoadCallback(
    LoadCallback raw_callback) {
  Cartridge::LoadCallback load_callback_with_debug_port = base::BindOnce(
      [](scoped_refptr<EmulatorImpl> this_impl, LoadCallback origin_callback,
         Cartridge::LoadResult load_result) {
        if (this_impl->debug_port_) {
          this_impl->debug_port_->OnRomLoaded(
              load_result.success, load_result.success
                                       ? this_impl->cartridge_->GetRomData()
                                       : nullptr);
        }

        // Set patch config for PPU
        this_impl->ppu_->SetPatch(this_impl->cartridge_->crc32());

        // Set mapper for buses.
        this_impl->cpu_bus_->SetMapper(this_impl->cartridge_->mapper());
        this_impl->ppu_bus_->SetMapper(this_impl->cartridge_->mapper());
        this_impl->cartridge_->mapper()->set_mirroring_changed_callback(
            base::BindRepeating(&PPUBus::UpdateMirroring,
                                base::Unretained(this_impl->ppu_bus_.get())));
        this_impl->cartridge_->mapper()->set_scanline_irq_callback(
            base::BindRepeating(&CPU::Interrupt,
                                base::Unretained(this_impl->cpu_.get()),
                                CPU::InterruptType::IRQ));

        // Reset CPU and PPU.
        this_impl->Reset(
            base::BindOnce(std::move(origin_callback), load_result.success));
      },
      base::RetainedRef(this), std::move(raw_callback));
  return load_callback_with_debug_port;
}

void EmulatorImpl::StepInternal() {
  apu_->increase_cycles();

  // https://www.nesdev.org/wiki/Cycle_reference_chart
  // PPU
  ppu_->Step();
  ppu_->Step();
  ppu_->Step();

  // CPU
  cpu_->Step();

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
  if (working_task_runner_->RunsTasksInCurrentSequence()) {
    Bytes data = SaveStateOnProperThread();
    std::move(callback).Run(std::move(data));
  } else {
    working_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&EmulatorImpl::SaveStateOnProperThread,
                       base::Unretained(this)),
        std::move(callback));
  }
}

void EmulatorImpl::LoadState(const Bytes& data, LoadCallback callback) {
  if (working_task_runner_->RunsTasksInCurrentSequence()) {
    bool result = LoadStateOnProperThread(data);
    std::move(callback).Run(result);
  } else {
    working_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&EmulatorImpl::LoadStateOnProperThread,
                       base::Unretained(this), data),
        std::move(callback));
  }
}

void EmulatorImpl::SetVolume(float volume) {
  apu_->SetVolume(volume);
}

float EmulatorImpl::GetVolume() {
  return apu_->GetVolume();
}

Byte EmulatorImpl::Read(Address address) {
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
  if (debug_port_) {
    debug_port_->OnPPUADDR(address);
  }
}

void EmulatorImpl::OnPPUScanlineStart(int scanline) {
  if (debug_port_) {
    debug_port_->OnScanlineStart(scanline);
  }
}

void EmulatorImpl::OnPPUScanlineEnd(int scanline) {
  if (debug_port_) {
    debug_port_->OnScanlineEnd(scanline);
  }
}

void EmulatorImpl::OnPPUFrameStart() {
  if (debug_port_) {
    debug_port_->OnFrameStart();
  }
}

void EmulatorImpl::OnPPUFrameEnd() {
  if (debug_port_) {
    debug_port_->OnFrameEnd();
  }
}

void EmulatorImpl::OnRenderReady(const Colors& swapbuffer) {
  // Render is ready, update APU state here.
  apu_->StepFrame();

  if (debug_port_) {
    debug_port_->OnNametableRenderReady();
  }

  if (io_devices_) {
    for (IODevices::RenderDevice* render_device :
         io_devices_->render_devices()) {
      CHECK(render_device);
      if (render_device->NeedRender()) {
        if (working_task_runner_->RunsTasksInCurrentSequence()) {
          render_device->Render(256, 240, swapbuffer);
        } else {
          emulator_task_runner_->PostTask(
              FROM_HERE, base::BindOnce(&IODevices::RenderDevice::Render,
                                        base::Unretained(render_device), 256,
                                        240, swapbuffer));
        }
      }
    }
  }
}

void EmulatorImpl::OnCPUNMI() {
  if (debug_port_)
    debug_port_->OnCPUNMI();
}

void EmulatorImpl::OnCPUBeforeStep(CPUDebugState& state) {
  if (debug_port_)
    debug_port_->OnCPUBeforeStep(state);
}

void EmulatorImpl::OnCPUStepped() {
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

Colors EmulatorImpl::GetCurrentFrame() {
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

}  // namespace nes
}  // namespace kiwi
