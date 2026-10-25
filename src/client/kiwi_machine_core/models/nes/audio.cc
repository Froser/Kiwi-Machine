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

#include "models/nes/audio.h"
#include "build/build_config.h"

#include <kiwi_nes.h>

NESAudio::NESAudio() : AudioBase(kBufferSize) {}

NESAudio::~NESAudio() = default;

void NESAudio::Initialize() {
#if BUILDFLAG(IS_SWITCH)
  InitializeMixerOutput(kiwi::nes::IODevices::AudioDevice::kFrequency, 1,
                        "NES");
#else
  InitializeAudioDevice(kiwi::nes::IODevices::AudioDevice::kFrequency, 1,
                        "NES");
#endif
}

void NESAudio::OnSampleArrived(kiwi::nes::Sample* samples, size_t count) {
  WriteSamples(samples, count);
}
