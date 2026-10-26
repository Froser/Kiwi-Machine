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

#ifndef EMULATOR_RENDER_DEVICE_H_
#define EMULATOR_RENDER_DEVICE_H_

#include "emulator/emulator_export.h"
#include "emulator/types.h"

namespace kiwi {
namespace emu {

class EMULATOR_EXPORT RenderDevice {
 public:
  using Buffer = Colors;

  RenderDevice();
  virtual ~RenderDevice();

  virtual bool NeedRender() = 0;
};

}  // namespace emu
}  // namespace kiwi

#endif  // EMULATOR_RENDER_DEVICE_H_
