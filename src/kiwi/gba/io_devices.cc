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

#include "gba/io_devices.h"

namespace kiwi {
namespace gba {

IODevices::InputDevice::InputDevice() = default;
IODevices::InputDevice::~InputDevice() = default;
IODevices::RenderDevice::RenderDevice() = default;
IODevices::RenderDevice::~RenderDevice() = default;
IODevices::AudioDevice::AudioDevice() = default;
IODevices::AudioDevice::~AudioDevice() = default;
IODevices::MotionDevice::MotionDevice() = default;
IODevices::MotionDevice::~MotionDevice() = default;
void IODevices::MotionDevice::Sample() {}
int32_t IODevices::MotionDevice::ReadTiltX() {
  return 0;
}
int32_t IODevices::MotionDevice::ReadTiltY() {
  return 0;
}
int32_t IODevices::MotionDevice::ReadGyroZ() {
  return 0;
}
IODevices::LuminanceDevice::LuminanceDevice() = default;
IODevices::LuminanceDevice::~LuminanceDevice() = default;
void IODevices::LuminanceDevice::Sample() {}
IODevices::RumbleDevice::RumbleDevice() = default;
IODevices::RumbleDevice::~RumbleDevice() = default;
IODevices::RTCDevice::RTCDevice() = default;
IODevices::RTCDevice::~RTCDevice() = default;
void IODevices::RTCDevice::Sample() {}

}  // namespace gba
}  // namespace kiwi
