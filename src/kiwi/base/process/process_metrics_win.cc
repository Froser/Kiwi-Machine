// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include "base/numerics/safe_conversions.h"

#include <windows.h>

namespace kiwi::base {

// This function uses the following mapping between MEMORYSTATUSEX and
// SystemMemoryInfoKB:
//   ullTotalPhys ==> total
//   ullAvailPhys ==> avail_phys
//   ullTotalPageFile ==> swap_total
//   ullAvailPageFile ==> swap_free
bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  MEMORYSTATUSEX mem_status;
  mem_status.dwLength = sizeof(mem_status);
  if (!::GlobalMemoryStatusEx(&mem_status))
    return false;

  meminfo->total = saturated_cast<int>(mem_status.ullTotalPhys / 1024);
  meminfo->avail_phys = saturated_cast<int>(mem_status.ullAvailPhys / 1024);
  meminfo->swap_total = saturated_cast<int>(mem_status.ullTotalPageFile / 1024);
  meminfo->swap_free = saturated_cast<int>(mem_status.ullAvailPageFile / 1024);

  return true;
}

}  // namespace kiwi::base
