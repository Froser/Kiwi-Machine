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

#ifndef GBA_MOTION_DEVICE_H_
#define GBA_MOTION_DEVICE_H_

#include <cstdint>

#include "emulator/gba/gba_export.h"

namespace kiwi {
namespace gba {

class GBA_EXPORT MotionDevice {
 public:
  MotionDevice();
  virtual ~MotionDevice();

  virtual void Sample();
  virtual int32_t ReadTiltX();
  virtual int32_t ReadTiltY();
  virtual int32_t ReadGyroZ();
};

}  // namespace gba
}  // namespace kiwi

#endif  // GBA_MOTION_DEVICE_H_
