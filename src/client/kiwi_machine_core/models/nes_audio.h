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

#include <array>
#include <atomic>

#include "models/nes_runtime.h"

class NESAudio : public kiwi::nes::IODevices::AudioDevice {
 public:
  explicit NESAudio(NESRuntimeID runtime_id);
  ~NESAudio() override;

 public:
  void Initialize();
  void Start();
  void Reset();

 private:
  static void ReadAudioBuffer(void* userdata, Uint8* stream, int len);

 private:
  void ResetBuffer();
  void ReadAudioBuffer(Uint8* stream, int count);
  void Write(kiwi::nes::Sample* samples, size_t count);

 protected:
  // kiwi::nes::IODevices::AudioDevice:
  void OnSampleArrived(kiwi::nes::Sample* samples, size_t count) override;

 private:
  enum {
    kBufferSize = 512,
    kBufferCount = 48,
  };

  NESRuntimeID runtime_id_ = 0;
  NESRuntime::Data* runtime_data_ = nullptr;
  SDL_AudioDeviceID audio_device_id_ = 0;
  SDL_AudioSpec audio_spec_;

  // Buffers - use atomic counters for simplified synchronization
  std::array<std::array<kiwi::nes::Sample, kBufferSize>, kBufferCount> buffers_;
  // Temporary accumulation buffer
  std::array<kiwi::nes::Sample, kBufferSize> temp_buffer_;
  // Current position in temp buffer
  size_t temp_pos_ = 0;
  // Next buffer index to write
  std::atomic<size_t> write_buf_{0};
  // Next buffer index to read
  std::atomic<size_t> read_buf_{0};
  // Number of filled buffers
  std::atomic<size_t> filled_count_{0};
};

#endif  // MODELS_NES_AUDIO_H
