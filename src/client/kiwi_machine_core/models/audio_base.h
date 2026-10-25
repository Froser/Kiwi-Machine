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

#ifndef MODELS_AUDIO_BASE_H_
#define MODELS_AUDIO_BASE_H_

#include <SDL.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <vector>

#include "build/build_config.h"
#include "emulator/types.h"

class AudioBase {
 public:
  virtual ~AudioBase();

  void Start();
  void Reset();

 protected:
  explicit AudioBase(size_t buffer_size, size_t buffer_count = 48);

  bool InitializeAudioDevice(int frequency,
                             int channels,
                             const char* system_name);
#if BUILDFLAG(IS_SWITCH)
  bool InitializeMixerOutput(int frequency,
                             int channels,
                             const char* system_name);
#endif
  size_t ReadSamples(kiwi::emu::Sample* output, size_t count);
  void WriteSamples(const kiwi::emu::Sample* samples, size_t count);

  const SDL_AudioSpec& audio_spec() const { return audio_spec_; }
  void set_audio_spec(const SDL_AudioSpec& audio_spec) {
    audio_spec_ = audio_spec;
  }

  virtual bool IsOutputActive() const;
  virtual void LockOutput();
  virtual void UnlockOutput();

 private:
  static void ReadAudioBuffer(void* userdata, Uint8* stream, int len);
  void ReadAudioBuffer(Uint8* stream, int count);
  void ResetBuffer();
#if BUILDFLAG(IS_SWITCH)
  static void MixAudioBuffer(void* userdata, Uint8* stream, int len);
  void MixIntoAudioBuffer(Uint8* stream, int count);
  void UnregisterMixerOutput();

  static constexpr size_t kMaxMixerSources = 2;
  static std::array<AudioBase*, kMaxMixerSources> mixer_sources_;
#endif

  const size_t buffer_size_;
  const size_t buffer_count_;
  SDL_AudioDeviceID audio_device_id_ = 0;
  SDL_AudioSpec audio_spec_ = {};
  std::vector<kiwi::emu::Sample> buffers_;
  std::vector<kiwi::emu::Sample> temp_buffer_;
  size_t temp_pos_ = 0;
  size_t read_pos_ = 0;
  std::atomic<size_t> write_buffer_{0};
  std::atomic<size_t> read_buffer_{0};
  std::atomic<size_t> filled_count_{0};
#if BUILDFLAG(IS_SWITCH)
  int mixer_source_channels_ = 0;
  bool mixer_output_registered_ = false;
  std::vector<kiwi::emu::Sample> mixer_input_buffer_;
  std::vector<kiwi::emu::Sample> mixer_output_buffer_;
#endif
};

#endif  // MODELS_AUDIO_BASE_H_
