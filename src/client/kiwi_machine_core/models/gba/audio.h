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

#ifndef MODELS_GBA_AUDIO_H_
#define MODELS_GBA_AUDIO_H_

#include "emulator/gba/io_devices.h"
#include "models/audio_base.h"

class GBAAudio : public AudioBase, public kiwi::gba::IODevices::AudioDevice {
 public:
  GBAAudio();
  ~GBAAudio() override;

  void Initialize();

 protected:
  void OnSampleArrived(kiwi::gba::Sample* samples, size_t count) override;

 private:
  static constexpr size_t kBufferSize = 1024;
};

#endif  // MODELS_GBA_AUDIO_H_
