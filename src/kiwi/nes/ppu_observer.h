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

#ifndef NES_PPU_OBSERVER_H_
#define NES_PPU_OBSERVER_H_

#include "nes/types.h"

namespace kiwi {
namespace nes {
class PPUObserver {
 public:
  PPUObserver();
  virtual ~PPUObserver();

  virtual void OnPPUStepped() {}
  virtual void OnPPUADDR(Address address) {}
  virtual void OnPPUScanlineStart(int scanline) {}
  virtual void OnPPUScanlineEnd(int scanline) {}
  virtual void OnPPUFrameStart() {}
  virtual void OnPPUFrameEnd() {}
  // If all visible scanlines are rendered, this method will be called.
  virtual void OnRenderReady(const Colors& swapbuffer) {}
};

}  // namespace core
}  // namespace kiwi

#endif  // NES_PPU_OBSERVER_H_
