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

#include "utility/audio_effects.h"

#include <SDL.h>
#include <SDL_mixer.h>
#include <string.h>

#include "build/kiwi_defines.h"

namespace {

#if !KIWI_MOBILE && !KIWI_WASM
Mix_Music* g_all_effects[static_cast<int>(audio_resources::AudioID::kLast)];
bool g_effect_disabled = false;

void LoadAudioEffectFromMemory(audio_resources::AudioID type,
                               const unsigned char* data,
                               size_t size) {
  int index = static_cast<int>(type);
  if (g_all_effects[index])
    Mix_FreeMusic(g_all_effects[index]);

  SDL_RWops* bg_res = SDL_RWFromMem(const_cast<unsigned char*>(data), size);
  g_all_effects[index] = Mix_LoadMUS_RW(bg_res, 1);
  if (!g_all_effects[index]) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Can't load music: %s",
                SDL_GetError());
  }
}
#endif

}  // namespace

void InitializeAudioEffects() {
#if !KIWI_MOBILE && !KIWI_WASM
  int flags = MIX_INIT_MP3;
  if (Mix_Init(flags) == flags) {
    // Is all effects loaded?
    if (g_all_effects[0])
      return;

    int audio_rate = MIX_DEFAULT_FREQUENCY;
    Uint16 audio_format = MIX_DEFAULT_FORMAT;
    int audio_channels = MIX_DEFAULT_CHANNELS;
    int audio_buffers = 4096;
    if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) <
        0) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s\n",
                   SDL_GetError());
      return;
    }

    for (int i = 0; i < static_cast<int>(audio_resources::AudioID::kLast);
         ++i) {
      audio_resources::AudioID aid = static_cast<audio_resources::AudioID>(i);
      size_t size_out;
      const auto* data = audio_resources::GetData(aid, &size_out);
      LoadAudioEffectFromMemory(aid, data, size_out);
    }
  } else {
    SDL_assert(!"Mix init failed.");
  }
#endif
}

void UninitializeAudioEffects() {
#if !KIWI_MOBILE && !KIWI_WASM
  for (Mix_Music* m : g_all_effects) {
    Mix_FreeMusic(m);
  }
  memset(g_all_effects, 0, sizeof(g_all_effects));
  Mix_CloseAudio();
  Mix_Quit();
#endif
}

void SetEffectVolume(float volume) {
#if !KIWI_MOBILE && !KIWI_WASM
  Mix_VolumeMusic(MIX_MAX_VOLUME * volume);
#endif
}

ScopedDisableEffect::ScopedDisableEffect() {
  SetEffectEnabled(false);
}

ScopedDisableEffect::~ScopedDisableEffect() {
  SetEffectEnabled(true);
}

#if !DISABLE_SOUND_EFFECTS
void PlayEffect(audio_resources::AudioID aid) {
#if !KIWI_MOBILE && !KIWI_WASM
  if (!g_effect_disabled) {
    int index = static_cast<int>(aid);
    if (g_all_effects[index]) {
      Mix_PausedMusic();
      Mix_PlayMusic(g_all_effects[index], 0);
    } else {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Can't find audio effect type of %d", index);
    }
  }
#endif
}

void SetEffectEnabled(bool enabled) {
#if !KIWI_MOBILE && !KIWI_WASM
  g_effect_disabled = !enabled;
#endif
}
#endif
