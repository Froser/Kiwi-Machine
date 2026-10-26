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

#include <set>

#include "emulator/io_devices.h"
#include "emulator/nes/audio_device.h"
#include "emulator/nes/nes_export.h"
#include "emulator/nes/render_device.h"
#include "emulator/nes/types.h"

namespace kiwi {
namespace nes {

class NES_EXPORT InputDevice : public emu::InputDevice {
 public:
  InputDevice();
  ~InputDevice() override;

  // Bit OR combination
  enum ZapperState {
    kNone,
    kLightSensed,
    kTriggered,
  };

  // Method for getting Zapper lightgun state.
  // Return value is the OR bit combination of ZapperState.
  virtual int GetZapperState() = 0;
};

// A collection of IO devices, such as inputs, renderers, etc.
class NES_EXPORT IODevices : public emu::IODevicesBase<IODevices,
                                                       nes::InputDevice,
                                                       RenderDevice,
                                                       AudioDevice> {
 public:
  using InputDevice = nes::InputDevice;

 private:
  using Base =
      emu::IODevicesBase<IODevices, InputDevice, RenderDevice, AudioDevice>;
  friend Base;

  InputDevice* input_device_ = nullptr;
  std::set<RenderDevice*> render_devices_;
  AudioDevice* audio_device_ = nullptr;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_IO_DEVICES_H_
