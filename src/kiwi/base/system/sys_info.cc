// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/system/sys_info.h"

namespace kiwi::base {

// static
uint64_t SysInfo::AmountOfPhysicalMemory() {
  return AmountOfPhysicalMemoryImpl();
}

// static
uint64_t SysInfo::AmountOfAvailablePhysicalMemory() {
  return AmountOfAvailablePhysicalMemoryImpl();
}

}  // namespace kiwi::base