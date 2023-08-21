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

#ifndef NES_APU_H_
#define NES_APU_H_

#include <chrono>

#include "base/functional/callback.h"
#include "nes/emulator_states.h"
#include "nes/types.h"
#include "third_party/nes_apu/Blip_Buffer.h"
#include "third_party/nes_apu/Nes_Apu.h"

namespace kiwi {
namespace nes {
class EmulatorImpl;
class Bus;
class APU : public Device, public EmulatorStates::SerializableState {
 public:
  enum AudioChannels {
    kNoChannel = 0,
    kSquare_1 = 1 << 0,
    kSquare_2 = 1 << 1,
    kTriangle = 1 << 2,
    kNoise = 1 << 3,
    kDMC = 1 << 4,
    kAll = kSquare_1 | kSquare_2 | kTriangle | kNoise | kDMC,
  };

  using IRQCallback = base::RepeatingClosure;

  explicit APU(EmulatorImpl* emulator, Bus* cpu_bus);
  ~APU() override;

  enum {
    kOutBufferConstantSize = 4096,
  };

 public:
  void increase_cycles() { ++cycles_; }

  void Reset();
  void StepFrame();
  void SetIRQCallback(IRQCallback irq_callback);
  void RunIRQCallback();

  void SetAudioChannels(int audio_channels);
  int GetAudioChannels();

  void SetVolume(float volume);
  float GetVolume();


  // Device:
  Byte Read(Address address) override;
  void Write(Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  int64_t cycles_ = 0;
  Nes_Apu apu_impl_;
  Blip_Buffer buffer_;
  EmulatorImpl* emulator_ = nullptr;
  Bus* cpu_bus_ = nullptr;
  IRQCallback irq_callback_;
  float volume_ = 1.f;
  blip_sample_t out_buffer_[kOutBufferConstantSize] = {0};
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_APU_H_
