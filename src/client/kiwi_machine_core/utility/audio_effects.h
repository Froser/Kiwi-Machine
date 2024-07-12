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

#ifndef UTILITY_AUDIO_EFFECTS_H_
#define UTILITY_AUDIO_EFFECTS_H_

#include "build/kiwi_defines.h"
#include "resources/audio_resources.h"

void InitializeAudioEffects();
void UninitializeAudioEffects();
void SetEffectVolume(float volume);

#if DISABLE_SOUND_EFFECTS
// If sound effects is disabled, PlayEffect() function will do nothing.
#define PlayEffect(type) []{}()
#else
void PlayEffect(audio_resources::AudioID type);
#endif

#endif  // UTILITY_AUDIO_EFFECTS_H_