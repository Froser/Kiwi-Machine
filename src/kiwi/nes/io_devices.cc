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

#include "nes/io_devices.h"

namespace kiwi {
namespace nes {

IODevices::InputDevice::InputDevice() = default;
IODevices::InputDevice::~InputDevice() = default;
IODevices::RenderDevice::RenderDevice() = default;
IODevices::RenderDevice::~RenderDevice() = default;
IODevices::AudioDevice::AudioDevice() = default;
IODevices::AudioDevice::~AudioDevice() = default;

}  // namespace nes
}  // namespace kiwi
