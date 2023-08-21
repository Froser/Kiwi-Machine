// Copyright (c) 2022 ByteDance Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// History: yuyisi created on September, 10, 2022

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
