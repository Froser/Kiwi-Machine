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

#ifndef GBA_IO_DEVICES_H_
#define GBA_IO_DEVICES_H_

#include <cstddef>
#include <cstdint>
#include <set>

#include "gba/gba_export.h"
#include "gba/types.h"

namespace kiwi {
namespace gba {

struct FrameData {
  int width = kFrameWidth;
  int height = kFrameHeight;
  uint64_t frame_number = 0;
  const Colors* pixels = nullptr;
};

// A collection of IO devices, such as inputs, renderers, and audio outputs.
class GBA_EXPORT IODevices {
 public:
  class GBA_EXPORT InputDevice {
   public:
    InputDevice();
    virtual ~InputDevice();

    virtual bool IsKeyDown(ControllerButton button) = 0;
  };

  class GBA_EXPORT RenderDevice {
   public:
    using Buffer = Colors;

    RenderDevice();
    virtual ~RenderDevice();

    virtual bool NeedRender() = 0;
    virtual void Render(const FrameData& frame) = 0;
  };

  class GBA_EXPORT AudioDevice {
   public:
    static constexpr int kFrequency = 44100;
    static constexpr int kChannels = 2;

    AudioDevice();
    virtual ~AudioDevice();

    // |samples| contains interleaved stereo samples.
    virtual void OnSampleArrived(Sample* samples, size_t count) = 0;
  };

  class GBA_EXPORT MotionDevice {
   public:
    MotionDevice();
    virtual ~MotionDevice();

    virtual void Sample();
    virtual int32_t ReadTiltX();
    virtual int32_t ReadTiltY();
    virtual int32_t ReadGyroZ();
  };

  class GBA_EXPORT LuminanceDevice {
   public:
    LuminanceDevice();
    virtual ~LuminanceDevice();

    virtual void Sample();
    virtual uint8_t ReadLuminance() = 0;
  };

  class GBA_EXPORT RumbleDevice {
   public:
    RumbleDevice();
    virtual ~RumbleDevice();

    virtual void SetRumble(bool enabled) = 0;
  };

  class GBA_EXPORT RTCDevice {
   public:
    RTCDevice();
    virtual ~RTCDevice();

    virtual void Sample();
    virtual int64_t GetUnixTimeSeconds() = 0;
  };

  void set_input_device(InputDevice* input_device) {
    input_device_ = input_device;
  }

  InputDevice* input_device() { return input_device_; }

  void add_render_device(RenderDevice* render_device) {
    render_devices_.insert(render_device);
  }

  void remove_render_device(RenderDevice* render_device) {
    render_devices_.erase(render_device);
  }

  const std::set<RenderDevice*>& render_devices() { return render_devices_; }

  void set_audio_device(AudioDevice* audio_device) {
    audio_device_ = audio_device;
  }

  AudioDevice* audio_device() { return audio_device_; }

  void set_motion_device(MotionDevice* motion_device) {
    motion_device_ = motion_device;
  }

  MotionDevice* motion_device() { return motion_device_; }

  void set_luminance_device(LuminanceDevice* luminance_device) {
    luminance_device_ = luminance_device;
  }

  LuminanceDevice* luminance_device() { return luminance_device_; }

  void set_rumble_device(RumbleDevice* rumble_device) {
    rumble_device_ = rumble_device;
  }

  RumbleDevice* rumble_device() { return rumble_device_; }

  void set_rtc_device(RTCDevice* rtc_device) { rtc_device_ = rtc_device; }

  RTCDevice* rtc_device() { return rtc_device_; }

 private:
  InputDevice* input_device_ = nullptr;
  std::set<RenderDevice*> render_devices_;
  AudioDevice* audio_device_ = nullptr;
  MotionDevice* motion_device_ = nullptr;
  LuminanceDevice* luminance_device_ = nullptr;
  RumbleDevice* rumble_device_ = nullptr;
  RTCDevice* rtc_device_ = nullptr;
};

}  // namespace gba
}  // namespace kiwi

#endif  // GBA_IO_DEVICES_H_
