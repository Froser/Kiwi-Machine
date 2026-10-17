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

#include "models/audio_base.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>

AudioBase::AudioBase(size_t buffer_size, size_t buffer_count)
    : buffer_size_(buffer_size),
      buffer_count_(buffer_count),
      buffers_(buffer_size * buffer_count),
      temp_buffer_(buffer_size) {
  SDL_assert(buffer_size_ > 0);
  SDL_assert(buffer_count_ > 0);
}

AudioBase::~AudioBase() {
  if (audio_device_id_) {
    SDL_CloseAudioDevice(audio_device_id_);
  }
}

bool AudioBase::InitializeAudioDevice(int frequency,
                                      int channels,
                                      const char* system_name) {
  Reset();
  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                 "Cannot open %s audio: SDL audio is not initialized",
                 system_name);
    return false;
  }

  SDL_assert(channels > 0);
  SDL_assert(buffer_size_ % static_cast<size_t>(channels) == 0);
  const size_t sample_frames = buffer_size_ / channels;
  SDL_assert(sample_frames <= std::numeric_limits<Uint16>::max());

  SDL_AudioSpec desired = {};
  desired.freq = frequency;
  desired.format = AUDIO_S16SYS;
  desired.channels = static_cast<Uint8>(channels);
  desired.callback = &AudioBase::ReadAudioBuffer;
  desired.samples = static_cast<Uint16>(sample_frames);
  desired.userdata = this;

  audio_device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &audio_spec_, 0);
  if (!audio_device_id_) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Error opening %s audio device: %s",
                 system_name, SDL_GetError());
    return false;
  }

  SDL_PauseAudioDevice(audio_device_id_, true);
  return true;
}

void AudioBase::Start() {
  if (audio_device_id_) {
    SDL_PauseAudioDevice(audio_device_id_, false);
  }
}

void AudioBase::Reset() {
  ResetBuffer();
}

bool AudioBase::IsOutputActive() const {
  return audio_device_id_ != 0;
}

void AudioBase::LockOutput() {
  SDL_LockAudioDevice(audio_device_id_);
}

void AudioBase::UnlockOutput() {
  SDL_UnlockAudioDevice(audio_device_id_);
}

void AudioBase::ResetBuffer() {
  const bool output_active = IsOutputActive();
  if (output_active) {
    LockOutput();
  }

  std::fill(buffers_.begin(), buffers_.end(), 0);
  std::fill(temp_buffer_.begin(), temp_buffer_.end(), 0);
  temp_pos_ = 0;
  read_pos_ = 0;
  write_buffer_.store(0, std::memory_order_relaxed);
  read_buffer_.store(0, std::memory_order_relaxed);
  filled_count_.store(0, std::memory_order_relaxed);

  if (output_active) {
    UnlockOutput();
  }
}

void AudioBase::ReadAudioBuffer(void* userdata, Uint8* stream, int len) {
  SDL_assert(userdata);
  static_cast<AudioBase*>(userdata)->ReadAudioBuffer(stream, len);
}

void AudioBase::ReadAudioBuffer(Uint8* stream, int count) {
  if (!SDL_AUDIO_ISLITTLEENDIAN(audio_spec_.format)) {
    static std::once_flag flag;
    std::call_once(flag, []() {
      SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Big endian is not supported yet.");
    });
    std::memset(stream, 0, count);
    return;
  }

  SDL_assert(count % sizeof(kiwi::emu::Sample) == 0);
  const size_t sample_count = count / sizeof(kiwi::emu::Sample);
  auto* output = reinterpret_cast<kiwi::emu::Sample*>(stream);
  const size_t samples_read = ReadSamples(output, sample_count);
  std::fill(output + samples_read, output + sample_count, 0);
}

size_t AudioBase::ReadSamples(kiwi::emu::Sample* output, size_t count) {
  size_t samples_read = 0;
  while (samples_read < count) {
    if (filled_count_.load(std::memory_order_acquire) == 0) {
      break;
    }

    const size_t current_read = read_buffer_.load(std::memory_order_relaxed);
    const size_t samples_to_copy =
        std::min(count - samples_read, buffer_size_ - read_pos_);
    std::memcpy(output + samples_read,
                buffers_.data() + current_read * buffer_size_ + read_pos_,
                samples_to_copy * sizeof(kiwi::emu::Sample));
    samples_read += samples_to_copy;
    read_pos_ += samples_to_copy;

    if (read_pos_ == buffer_size_) {
      read_pos_ = 0;
      read_buffer_.store((current_read + 1) % buffer_count_,
                         std::memory_order_release);
      filled_count_.fetch_sub(1, std::memory_order_release);
    }
  }
  return samples_read;
}

void AudioBase::WriteSamples(const kiwi::emu::Sample* samples, size_t count) {
  if (!IsOutputActive()) {
    return;
  }

  while (count > 0) {
    const size_t samples_to_copy = std::min(count, buffer_size_ - temp_pos_);
    std::memcpy(temp_buffer_.data() + temp_pos_, samples,
                samples_to_copy * sizeof(kiwi::emu::Sample));
    temp_pos_ += samples_to_copy;
    samples += samples_to_copy;
    count -= samples_to_copy;

    if (temp_pos_ == buffer_size_) {
      if (filled_count_.load(std::memory_order_acquire) >= buffer_count_) {
        temp_pos_ = 0;
        return;
      }

      const size_t current_write =
          write_buffer_.load(std::memory_order_relaxed);
      std::memcpy(buffers_.data() + current_write * buffer_size_,
                  temp_buffer_.data(),
                  buffer_size_ * sizeof(kiwi::emu::Sample));
      write_buffer_.store((current_write + 1) % buffer_count_,
                          std::memory_order_release);
      filled_count_.fetch_add(1, std::memory_order_release);
      temp_pos_ = 0;
    }
  }
}
