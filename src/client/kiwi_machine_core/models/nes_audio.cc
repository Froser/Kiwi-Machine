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

#include "models/nes_audio.h"

#include <kiwi_nes.h>

#include <mutex>

NESAudio::NESAudio(NESRuntimeID runtime_id) : runtime_id_(runtime_id) {}

NESAudio::~NESAudio() {
  if (audio_device_id_)
    SDL_CloseAudioDevice(audio_device_id_);
}

void NESAudio::Reset() {
  ResetBuffer();
}

void NESAudio::Initialize() {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id_);
  SDL_assert(runtime_data_);
  SDL_assert(runtime_data_->emulator);

  ResetBuffer();

  if (SDL_WasInit(SDL_INIT_AUDIO)) {
    SDL_AudioSpec as;
    as.freq = kiwi::nes::IODevices::AudioDevice::kFrequency;
    as.format = AUDIO_S16SYS;
    as.channels = 1;
    as.silence = 0;
    as.callback = &NESAudio::ReadAudioBuffer;
    as.samples = kBufferSize;
    as.userdata = this;

    audio_device_id_ = SDL_OpenAudioDevice(nullptr, 0, &as, &audio_spec_, 0);
    SDL_PauseAudioDevice(audio_device_id_, true);
    if (!audio_device_id_) {
      SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Error in open audio device: %s",
                   SDL_GetError());
    }
  }
}

void NESAudio::Start() {
  if (audio_device_id_)
    SDL_PauseAudioDevice(audio_device_id_, false);
}

void NESAudio::ResetBuffer() {
  SDL_LockAudioDevice(audio_device_id_);

  // Clear all buffers
  for (auto& buf : buffers_) {
    buf.fill(0);
  }
  temp_buffer_.fill(0);
  temp_pos_ = 0;

  // Reset atomic counters
  write_buf_.store(0, std::memory_order_relaxed);
  read_buf_.store(0, std::memory_order_relaxed);
  filled_count_.store(0, std::memory_order_relaxed);

  SDL_UnlockAudioDevice(audio_device_id_);
}

void NESAudio::ReadAudioBuffer(void* userdata, Uint8* stream, int len) {
  SDL_assert(userdata);
  NESAudio* audio = reinterpret_cast<NESAudio*>(userdata);
  audio->ReadAudioBuffer(stream, len);
}

void NESAudio::ReadAudioBuffer(Uint8* stream, int count) {
  // Check if there are any filled buffers available
  size_t current_filled = filled_count_.load(std::memory_order_acquire);
  if (current_filled > 0) {
    size_t current_read = read_buf_.load(std::memory_order_relaxed);

    // TODO MSB is not supported yet.
    if (SDL_AUDIO_ISLITTLEENDIAN(audio_spec_.format)) {
      memcpy(stream, buffers_[current_read].data(), count);

      // Update read index
      read_buf_.store((current_read + 1) % kBufferCount,
                      std::memory_order_release);

      // Decrement filled count
      filled_count_.fetch_sub(1, std::memory_order_release);
    } else {
      static std::once_flag flag;
      std::call_once(flag, [this]() {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Big endian is not supported yet.");
      });
      // Even if big endian is not supported, update counters to avoid deadlock
      read_buf_.store((current_read + 1) % kBufferCount,
                      std::memory_order_release);
      filled_count_.fetch_sub(1, std::memory_order_release);
    }
  } else {
    // No data available, play silence
    memset(stream, 0, count);
  }
}

void NESAudio::Write(kiwi::nes::Sample* samples, size_t count) {
  if (!audio_device_id_)
    return;

  const kiwi::nes::Sample* in = samples;
  while (count > 0) {
    // Check how much space is left in the temp buffer
    size_t space_in_temp = kBufferSize - temp_pos_;
    size_t to_copy = (count < space_in_temp) ? count : space_in_temp;

    // Copy to temp buffer
    memcpy(temp_buffer_.data() + temp_pos_, in,
           to_copy * sizeof(kiwi::nes::Sample));
    temp_pos_ += to_copy;
    in += to_copy;
    count -= to_copy;

    // If temp buffer is full, commit to ring buffer
    if (temp_pos_ >= kBufferSize) {
      // Check if there's space available
      size_t current_filled = filled_count_.load(std::memory_order_acquire);
      if (current_filled >= kBufferCount) {
        // Buffer full, clear temp buffer and drop data
        temp_pos_ = 0;
        return;
      }

      size_t current_write = write_buf_.load(std::memory_order_relaxed);

      // Copy temp buffer to ring buffer
      memcpy(buffers_[current_write].data(), temp_buffer_.data(),
             kBufferSize * sizeof(kiwi::nes::Sample));

      // Update write index
      write_buf_.store((current_write + 1) % kBufferCount,
                       std::memory_order_release);

      // Increment filled count
      filled_count_.fetch_add(1, std::memory_order_release);

      // Clear temp buffer
      temp_pos_ = 0;
    }
  }
}

void NESAudio::OnSampleArrived(kiwi::nes::Sample* samples, size_t count) {
  Write(samples, count);
}
