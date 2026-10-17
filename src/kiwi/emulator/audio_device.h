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

#ifndef EMULATOR_AUDIO_DEVICE_H_
#define EMULATOR_AUDIO_DEVICE_H_

#include <cstddef>

#include "emulator/emulator_export.h"
#include "emulator/types.h"

namespace kiwi {
namespace emu {

class EMULATOR_EXPORT AudioDevice {
 public:
  static constexpr int kFrequency = 44100;

  AudioDevice();
  virtual ~AudioDevice();

  virtual void OnSampleArrived(Sample* samples, size_t count) = 0;
};

}  // namespace emu
}  // namespace kiwi

#endif  // EMULATOR_AUDIO_DEVICE_H_
