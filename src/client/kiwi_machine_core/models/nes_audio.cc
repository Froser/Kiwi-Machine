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

#if KIWI_SWITCH
#include <SDL_mixer.h>
#endif
#include <kiwi_nes.h>

#include <algorithm>
#include <mutex>

NESAudio::NESAudio(NESRuntimeID runtime_id) : runtime_id_(runtime_id) {}

NESAudio::~NESAudio() {
#if KIWI_SWITCH
  if (post_mix_registered_)
    Mix_SetPostMix(nullptr, nullptr);
#else
  if (audio_device_id_)
    SDL_CloseAudioDevice(audio_device_id_);
#endif
}

void NESAudio::Reset() {
  ResetBuffer();
}

void NESAudio::Initialize() {
  runtime_data_ = NESRuntime::GetInstance()->GetDataById(runtime_id_);
  SDL_assert(runtime_data_);
  SDL_assert(runtime_data_->emulator);

  ResetBuffer();

  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                 "Cannot open NES audio: SDL audio is not initialized");
    return;
  }

#if KIWI_SWITCH
  int frequency = 0;
  Uint16 format = 0;
  int channels = 0;
  if (!Mix_QuerySpec(&frequency, &format, &channels)) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                 "Cannot mix NES audio: SDL_mixer is not initialized");
    return;
  }
  if (frequency != kiwi::nes::IODevices::AudioDevice::kFrequency ||
      format != AUDIO_S16SYS || channels != 2) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                 "Unsupported Switch mixer format: %d Hz, 0x%x, %d channels",
                 frequency, format, channels);
    return;
  }

  audio_spec_ = {};
  audio_spec_.freq = frequency;
  audio_spec_.format = format;
  audio_spec_.channels = channels;
  post_mix_registered_ = true;
  Mix_SetPostMix(&NESAudio::MixAudioBuffer, this);
#else
  SDL_AudioSpec desired = {};
  desired.freq = kiwi::nes::IODevices::AudioDevice::kFrequency;
  desired.format = AUDIO_S16SYS;
  desired.channels = 1;
  desired.callback = &NESAudio::ReadAudioBuffer;
  desired.samples = kBufferSize;
  desired.userdata = this;

  audio_device_id_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &audio_spec_, 0);
  if (!audio_device_id_) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "Error opening NES audio device: %s",
                 SDL_GetError());
    return;
  }

  SDL_PauseAudioDevice(audio_device_id_, true);
#endif
}

void NESAudio::Start() {
#if !KIWI_SWITCH
  if (audio_device_id_)
    SDL_PauseAudioDevice(audio_device_id_, false);
#endif
}

void NESAudio::ResetBuffer() {
#if KIWI_SWITCH
  const bool restore_post_mix = post_mix_registered_;
  if (restore_post_mix)
    Mix_SetPostMix(nullptr, nullptr);
#else
  if (audio_device_id_)
    SDL_LockAudioDevice(audio_device_id_);
#endif

  // Clear all buffers
  for (auto& buf : buffers_) {
    buf.fill(0);
  }
  temp_buffer_.fill(0);
  temp_pos_ = 0;
  read_pos_ = 0;

  // Reset atomic counters
  write_buf_.store(0, std::memory_order_relaxed);
  read_buf_.store(0, std::memory_order_relaxed);
  filled_count_.store(0, std::memory_order_relaxed);

#if KIWI_SWITCH
  if (restore_post_mix)
    Mix_SetPostMix(&NESAudio::MixAudioBuffer, this);
#else
  if (audio_device_id_)
    SDL_UnlockAudioDevice(audio_device_id_);
#endif
}

void NESAudio::ReadAudioBuffer(void* userdata, Uint8* stream, int len) {
  SDL_assert(userdata);
  NESAudio* audio = reinterpret_cast<NESAudio*>(userdata);
  audio->ReadAudioBuffer(stream, len);
}

void NESAudio::ReadAudioBuffer(Uint8* stream, int count) {
  if (!SDL_AUDIO_ISLITTLEENDIAN(audio_spec_.format)) {
    static std::once_flag flag;
    std::call_once(flag, []() {
      SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "Big endian is not supported yet.");
    });
    memset(stream, 0, count);
    return;
  }

  SDL_assert(count % sizeof(kiwi::nes::Sample) == 0);
  const size_t sample_count = count / sizeof(kiwi::nes::Sample);
  auto* output = reinterpret_cast<kiwi::nes::Sample*>(stream);
  const size_t samples_read = ReadSamples(output, sample_count);
  std::fill(output + samples_read, output + sample_count, 0);
}

#if KIWI_SWITCH
void NESAudio::MixAudioBuffer(void* userdata, Uint8* stream, int len) {
  SDL_assert(userdata);
  NESAudio* audio = reinterpret_cast<NESAudio*>(userdata);
  audio->MixAudioBuffer(stream, len);
}

void NESAudio::MixAudioBuffer(Uint8* stream, int count) {
  constexpr size_t kMixerChannels = 2;
  constexpr size_t kMixerFrameSize = sizeof(kiwi::nes::Sample) * kMixerChannels;
  SDL_assert(count % kMixerFrameSize == 0);

  size_t frames_remaining = count / kMixerFrameSize;
  while (frames_remaining > 0) {
    const size_t frames_to_mix =
        std::min(frames_remaining, static_cast<size_t>(kBufferSize));
    const size_t frames_read =
        ReadSamples(mixer_mono_buffer_.data(), frames_to_mix);
    for (size_t i = 0; i < frames_read; ++i) {
      mixer_stereo_buffer_[i * 2] = mixer_mono_buffer_[i];
      mixer_stereo_buffer_[i * 2 + 1] = mixer_mono_buffer_[i];
    }

    const size_t bytes_to_mix = frames_read * kMixerFrameSize;
    SDL_MixAudioFormat(stream,
                       reinterpret_cast<Uint8*>(mixer_stereo_buffer_.data()),
                       audio_spec_.format, bytes_to_mix, SDL_MIX_MAXVOLUME);
    stream += frames_to_mix * kMixerFrameSize;
    frames_remaining -= frames_to_mix;
  }
}
#endif

size_t NESAudio::ReadSamples(kiwi::nes::Sample* output, size_t count) {
  size_t samples_read = 0;
  while (samples_read < count) {
    const size_t current_filled = filled_count_.load(std::memory_order_acquire);
    if (current_filled == 0)
      break;

    size_t current_read = read_buf_.load(std::memory_order_relaxed);
    const size_t samples_to_copy =
        std::min(count - samples_read, kBufferSize - read_pos_);
    memcpy(output + samples_read, buffers_[current_read].data() + read_pos_,
           samples_to_copy * sizeof(kiwi::nes::Sample));
    samples_read += samples_to_copy;
    read_pos_ += samples_to_copy;

    if (read_pos_ == kBufferSize) {
      read_pos_ = 0;
      read_buf_.store((current_read + 1) % kBufferCount,
                      std::memory_order_release);
      filled_count_.fetch_sub(1, std::memory_order_release);
    }
  }
  return samples_read;
}

void NESAudio::Write(kiwi::nes::Sample* samples, size_t count) {
#if KIWI_SWITCH
  if (!post_mix_registered_)
    return;
#else
  if (!audio_device_id_)
    return;
#endif

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
