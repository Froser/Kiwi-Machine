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

#ifndef NES_IO_DEVICES_H_
#define NES_IO_DEVICES_H_

#include <cstdint>
#include <set>

#include "nes/nes_export.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
// A collection of IO devices, such as inputs, renderers, etc.
class NES_EXPORT IODevices {
 public:
  class NES_EXPORT InputDevice {
   public:
    InputDevice();
    virtual ~InputDevice();

   public:
    virtual bool IsKeyDown(int controller_id, ControllerButton button) = 0;
  };

  class NES_EXPORT RenderDevice {
   public:
    using BGRA = uint32_t;
    using Buffer = std::vector<BGRA>;

   public:
    RenderDevice();
    virtual ~RenderDevice();

   public:
    virtual void Render(int width, int height, const Buffer& buffer) = 0;
    virtual bool NeedRender() = 0;
  };

  class NES_EXPORT AudioDevice {
   public:
    enum {
      kFrequency = 44100,
#if BUILDFLAG(IS_WIN)
      kBufferMS = 0,
#else
      kBufferMS = 65000,
#endif
    };

    AudioDevice();
    virtual ~AudioDevice();

   public:
    virtual void OnSampleArrived(Sample* samples, size_t count) = 0;
  };

  // Sets/Gets the input delegate, to handle input state.
  void set_input_device(InputDevice* input_device) {
    input_device_ = input_device;
  }

  InputDevice* input_device() { return input_device_; }

  // Adds a render device to the emulator.
  void add_render_device(RenderDevice* render_device) {
    render_devices_.insert(render_device);
  }

  const std::set<RenderDevice*>& render_devices() { return render_devices_; }

  void set_audio_device(AudioDevice* audio_device) {
    audio_device_ = audio_device;
  }

  AudioDevice* audio_device() { return audio_device_; }

 private:
  InputDevice* input_device_ = nullptr;
  std::set<RenderDevice*> render_devices_;
  AudioDevice* audio_device_ = nullptr;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_IO_DEVICES_H_
