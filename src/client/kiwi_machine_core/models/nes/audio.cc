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

#if BUILDFLAG(IS_SWITCH)
#include <SDL_mixer.h>
#endif
#include <kiwi_nes.h>

#include <algorithm>

NESAudio::NESAudio() : AudioBase(kBufferSize) {}

NESAudio::~NESAudio() {
#if BUILDFLAG(IS_SWITCH)
  if (post_mix_registered_)
    Mix_SetPostMix(nullptr, nullptr);
#endif
}

void NESAudio::Initialize() {
#if BUILDFLAG(IS_SWITCH)
  Reset();
  if (!SDL_WasInit(SDL_INIT_AUDIO)) {
    SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                 "Cannot open NES audio: SDL audio is not initialized");
    return;
  }

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

  SDL_AudioSpec audio_spec = {};
  audio_spec.freq = frequency;
  audio_spec.format = format;
  audio_spec.channels = channels;
  set_audio_spec(audio_spec);
  post_mix_registered_ = true;
  Mix_SetPostMix(&NESAudio::MixAudioBuffer, this);
#else
  InitializeAudioDevice(kiwi::nes::IODevices::AudioDevice::kFrequency, 1,
                        "NES");
#endif
}

#if BUILDFLAG(IS_SWITCH)
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
                       audio_spec().format, bytes_to_mix, SDL_MIX_MAXVOLUME);
    stream += frames_to_mix * kMixerFrameSize;
    frames_remaining -= frames_to_mix;
  }
}

bool NESAudio::IsOutputActive() const {
  return post_mix_registered_;
}

void NESAudio::LockOutput() {
  Mix_SetPostMix(nullptr, nullptr);
}

void NESAudio::UnlockOutput() {
  Mix_SetPostMix(&NESAudio::MixAudioBuffer, this);
}
#endif

void NESAudio::OnSampleArrived(kiwi::nes::Sample* samples, size_t count) {
  WriteSamples(samples, count);
}
