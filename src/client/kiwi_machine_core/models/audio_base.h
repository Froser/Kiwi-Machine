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

#include <atomic>
#include <cstddef>
#include <vector>

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
};

#endif  // MODELS_AUDIO_BASE_H_
