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

#ifndef EMULATOR_IO_DEVICES_H_
#define EMULATOR_IO_DEVICES_H_

#include <set>

#include "emulator/audio_device.h"
#include "emulator/input_device.h"
#include "emulator/render_device.h"

namespace kiwi {
namespace emu {

template <typename Derived,
          typename InputDeviceType,
          typename RenderDeviceType,
          typename AudioDeviceType>
class IODevicesBase {
 public:
  using InputDevice = InputDeviceType;
  using RenderDevice = RenderDeviceType;
  using AudioDevice = AudioDeviceType;

  void set_input_device(InputDevice* input_device) {
    derived().input_device_ = input_device;
  }

  InputDevice* input_device() { return derived().input_device_; }

  void add_render_device(RenderDevice* render_device) {
    derived().render_devices_.insert(render_device);
  }

  void remove_render_device(RenderDevice* render_device) {
    derived().render_devices_.erase(render_device);
  }

  const std::set<RenderDevice*>& render_devices() {
    return derived().render_devices_;
  }

  void set_audio_device(AudioDevice* audio_device) {
    derived().audio_device_ = audio_device;
  }

  AudioDevice* audio_device() { return derived().audio_device_; }

 private:
  Derived& derived() { return static_cast<Derived&>(*this); }
};

}  // namespace emu
}  // namespace kiwi

#endif  // EMULATOR_IO_DEVICES_H_
