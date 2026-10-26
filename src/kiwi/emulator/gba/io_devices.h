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

#include <set>

#include "emulator/gba/audio_device.h"
#include "emulator/gba/gba_export.h"
#include "emulator/gba/luminance_device.h"
#include "emulator/gba/motion_device.h"
#include "emulator/gba/render_device.h"
#include "emulator/gba/rtc_device.h"
#include "emulator/gba/rumble_device.h"
#include "emulator/io_devices.h"

namespace kiwi {
namespace gba {

// A collection of IO devices, such as inputs, renderers, and audio outputs.
class GBA_EXPORT IODevices : public emu::IODevicesBase<IODevices,
                                                       emu::InputDevice,
                                                       RenderDevice,
                                                       AudioDevice> {
 public:
  using InputDevice = emu::InputDevice;
  using LuminanceDevice = gba::LuminanceDevice;
  using MotionDevice = gba::MotionDevice;
  using RTCDevice = gba::RTCDevice;
  using RumbleDevice = gba::RumbleDevice;

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
  using Base =
      emu::IODevicesBase<IODevices, InputDevice, RenderDevice, AudioDevice>;
  friend Base;

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
