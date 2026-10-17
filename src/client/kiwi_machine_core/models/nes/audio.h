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

#ifndef MODELS_NES_AUDIO_H_
#define MODELS_NES_AUDIO_H_

#include <SDL.h>
#include <kiwi_nes.h>
#include "build/build_config.h"

#if BUILDFLAG(IS_SWITCH)
#include <array>
#endif

#include "build/kiwi_defines.h"
#include "models/audio_base.h"

class NESAudio : public AudioBase, public kiwi::nes::IODevices::AudioDevice {
 public:
  NESAudio();
  ~NESAudio() override;

  void Initialize();

#if BUILDFLAG(IS_SWITCH)
 private:
  static void MixAudioBuffer(void* userdata, Uint8* stream, int len);
  void MixAudioBuffer(Uint8* stream, int count);
  bool IsOutputActive() const override;
  void LockOutput() override;
  void UnlockOutput() override;
#endif

 protected:
  // kiwi::nes::IODevices::AudioDevice:
  void OnSampleArrived(kiwi::nes::Sample* samples, size_t count) override;

 private:
  static constexpr size_t kBufferSize = 512;

#if BUILDFLAG(IS_SWITCH)
  bool post_mix_registered_ = false;
  std::array<kiwi::nes::Sample, kBufferSize> mixer_mono_buffer_;
  std::array<kiwi::nes::Sample, kBufferSize * 2> mixer_stereo_buffer_;
#endif
};

#endif  // MODELS_NES_AUDIO_H_
