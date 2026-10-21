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

#ifndef NES_AUDIO_DEVICE_H_
#define NES_AUDIO_DEVICE_H_

#include "build/build_config.h"
#include "emulator/audio_device.h"
#include "emulator/nes/nes_export.h"

namespace kiwi {
namespace nes {

class NES_EXPORT AudioDevice : public emu::AudioDevice {
 public:
  enum {
#if BUILDFLAG(IS_WIN)
    kBufferMS = 0,
#elif BUILDFLAG(IS_WASM)
    kBufferMS = 1250,
#elif BUILDFLAG(IS_ANDROID)
    kBufferMS = 1000,
#else
    kBufferMS = 65000,
#endif
  };

  AudioDevice();
  ~AudioDevice() override;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_AUDIO_DEVICE_H_
