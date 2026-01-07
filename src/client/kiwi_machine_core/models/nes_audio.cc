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

namespace {
constexpr size_t kBufferSize = 512;
constexpr size_t kBufferCount = 12;
}  // namespace

NESAudio::NESAudio(NESRuntimeID runtime_id) : runtime_id_(runtime_id) {}

NESAudio::~NESAudio() {
  if (audio_device_id_)
    SDL_CloseAudioDevice(audio_device_id_);

  if (free_sem_)
    SDL_DestroySemaphore(free_sem_);
}

void NESAudio::Reset() {
  ResetBuffer();
}

void NESAudio::Initialize() {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id_);
  SDL_assert(runtime_data_);
  SDL_assert(runtime_data_->emulator);

  ResetBuffer();

  free_sem_ = SDL_CreateSemaphore(kBufferCount - 1);

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
  if (free_sem_)
    SDL_DestroySemaphore(free_sem_);
  free_sem_ = SDL_CreateSemaphore(kBufferCount - 1);

  buffer_.resize(kBufferCount * kBufferSize);
  write_buf_ = 0;
  write_pos_ = 0;
  read_buf_ = 0;
  SDL_UnlockAudioDevice(audio_device_id_);
}

void NESAudio::ReadAudioBuffer(void* userdata, Uint8* stream, int len) {
  SDL_assert(userdata);
  NESAudio* audio = reinterpret_cast<NESAudio*>(userdata);
  audio->ReadAudioBuffer(stream, len);
}

void NESAudio::ReadAudioBuffer(Uint8* stream, int count) {
  if (SDL_SemValue(free_sem_) < kBufferCount - 1) {
    // TODO MSB is not supported yet.
    if (SDL_AUDIO_ISLITTLEENDIAN(audio_spec_.format)) {
      memcpy(stream, Buffer(read_buf_), count);
      read_buf_ = (read_buf_ + 1) % kBufferCount;
      SDL_SemPost(free_sem_);
    } else {
      std::once_flag flag;
      std::call_once(flag, [this]() {
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Big endian is not supported yet.");
      });
    }
  } else {
    memset(stream, 0, count);
  }
}

kiwi::nes::Sample* NESAudio::Buffer(size_t index) {
  SDL_assert(index < kBufferCount);
  return buffer_.data() + index * kBufferSize;
}

void NESAudio::Write(kiwi::nes::Sample* samples, size_t count) {
  if (!audio_device_id_)
    return;

  const kiwi::nes::Sample* in = samples;
  while (count) {
    int n = static_cast<int>(kBufferSize - write_pos_);
    if (n > count)
      n = count;

    memcpy(Buffer(write_buf_) + write_pos_, in, n * sizeof(kiwi::nes::Sample));
    in += n;
    write_pos_ += n;
    count -= n;

    if (write_pos_ >= kBufferSize) {
      write_pos_ = 0;
      write_buf_ = (write_buf_ + 1) % kBufferCount;
      SDL_SemWait(free_sem_);
    }
  }
}

void NESAudio::OnSampleArrived(kiwi::nes::Sample* samples, size_t count) {
  Write(samples, count);
}