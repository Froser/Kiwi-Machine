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

#include "nes/apu.h"

#include "base/logging.h"
#include "nes/cpu_bus.h"
#include "nes/emulator_impl.h"
#include "nes/io_devices.h"
#include "nes/registers.h"
#include "third_party/nes_apu/apu_snapshot.h"

namespace kiwi {
namespace nes {
namespace {
constexpr long kNTSCClockRate = 1789773;
constexpr int kAPUNTSCFrequency = 60;  // NTSC 60Hz
constexpr auto kAPUFrameDuration =
    std::chrono::milliseconds(1000 / kAPUNTSCFrequency);

int ReadDMC(void* user_data, cpu_addr_t address) {
  CPUBus* cpu_bus = reinterpret_cast<CPUBus*>(user_data);
  return cpu_bus->Read(address);
}

void IRQNotifier(void* user_data) {
  APU* apu = reinterpret_cast<APU*>(user_data);
  CHECK(apu);
  apu->RunIRQCallback();
}
}  // namespace

APU::APU(EmulatorImpl* emulator, CPUBus* cpu_bus)
    : emulator_(emulator), cpu_bus_(cpu_bus) {
  CHECK(emulator_);
  CHECK(cpu_bus_);
  apu_impl_.reset(false);
  buffer_.sample_rate(IODevices::AudioDevice::kFrequency,
                      IODevices::AudioDevice::kBufferMS);
  buffer_.clock_rate(kNTSCClockRate);
  apu_impl_.output(&buffer_);
  apu_impl_.dmc_reader(&ReadDMC, cpu_bus_);
}

APU::~APU() = default;

void APU::Reset() {
  // When APU implementation is reset, its inner counter will be reset to zero,
  // so clear_cycles() should be called as well.
  cycles_ = 0;
  apu_impl_.reset();
  buffer_.clear();
}

void APU::StepFrame() {
  // APU runs at kNTSCFrequency. If frame rendered to fast, just return and do
  // nothing.
  apu_impl_.end_frame(cycles_);
  buffer_.end_frame(cycles_);
  cycles_ = 0;

  while (buffer_.samples_avail() >= kOutBufferConstantSize) {
    size_t count = buffer_.read_samples(out_buffer_, kOutBufferConstantSize);
    if (emulator_->GetIODevices() &&
        emulator_->GetIODevices()->audio_device()) {
      emulator_->GetIODevices()->audio_device()->OnSampleArrived(out_buffer_,
                                                                 count);
    }
  }
}

void APU::SetIRQCallback(IRQCallback irq_callback) {
  irq_callback_ = irq_callback;
  apu_impl_.irq_notifier(IRQNotifier, this);
}

void APU::RunIRQCallback() {
  if (irq_callback_)
    irq_callback_.Run();
}

void APU::SetVolume(float volume) {
  volume_ = volume;
  apu_impl_.volume(volume);
}

float APU::GetVolume() {
  return volume_;
}

void APU::SetAudioChannels(int audio_channels) {
  apu_impl_.set_audio_channels(audio_channels);
}

int APU::GetAudioChannels() {
  return apu_impl_.audio_channels();
}

// Device:
Byte APU::Read(Address address) {
  if (address == 0x4015) {
    return apu_impl_.read_status(cycles_);
  }

  LOG(WARNING) << "Address $" << Hex<16>{address}
               << " is not handled for reading.";
  return 0;
}

void APU::Write(Address address, Byte value) {
  apu_impl_.write_register(cycles_, address, value);
}

void APU::Serialize(EmulatorStates::SerializableStateData& data) {
  apu_snapshot_t state;
  apu_impl_.save_snapshot(&state);
  data.WriteData(state);
}

bool APU::Deserialize(const EmulatorStates::Header& header,
                      EmulatorStates::DeserializableStateData& data) {
  if (header.version == 1) {
    Reset();
    apu_snapshot_t state;
    data.ReadData(&state);
    apu_impl_.load_snapshot(state);
    return true;
  }
  return false;
}

}  // namespace nes
}  // namespace kiwi
