// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYSTEM_SYS_INFO_H_
#define BASE_SYSTEM_SYS_INFO_H_

#include <stdint.h>
#include <string>

#include "base/base_export.h"

namespace kiwi::base {

class BASE_EXPORT SysInfo {
 public:
  // Return the number of bytes of physical memory on the current machine.
  // If low-end device mode is manually enabled via command line flag, this
  // will return the lesser of the actual physical memory, or 512MB.
  static uint64_t AmountOfPhysicalMemory();

  // Return the number of bytes of current available physical memory on the
  // machine.
  // (The amount of memory that can be allocated without any significant
  // impact on the system. It can lead to freeing inactive file-backed
  // and/or speculative file-backed memory).
  static uint64_t AmountOfAvailablePhysicalMemory();

  // Returns the version of the host operating system.
  static std::string OperatingSystemVersion();
  // Retrieves detailed numeric values for the OS version.
  // DON'T USE THIS ON THE MAC OR WINDOWS to determine the current OS release
  // for OS version-specific feature checks and workarounds. If you must use an
  // OS version check instead of a feature check, use
  // base::mac::MacOSVersion()/MacOSMajorVersion() family from
  // base/mac/mac_util.h, or base::win::GetVersion() from
  // base/win/windows_version.h.
  static void OperatingSystemVersionNumbers(int32_t* major_version,
                                            int32_t* minor_version,
                                            int32_t* bugfix_version);
 private:
  static uint64_t AmountOfPhysicalMemoryImpl();
  static uint64_t AmountOfAvailablePhysicalMemoryImpl();
};

}  // namespace kiwi::base

#endif  // BASE_SYSTEM_SYS_INFO_H_