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

#ifndef NES_CPU_OBSERVER_H_
#define NES_CPU_OBSERVER_H_

#include "nes/types.h"

namespace kiwi {
namespace nes {
struct CPUDebugState;
class CPUObserver {
 public:
  CPUObserver();
  virtual ~CPUObserver();

  virtual void OnCPUNMI() {}
  // Returns false to inhibit this instruction.
  virtual void OnCPUBeforeStep(CPUDebugState& state) {}
  virtual void OnCPUStepped() {}
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_CPU_OBSERVER_H_
