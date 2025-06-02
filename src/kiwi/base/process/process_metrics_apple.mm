// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#import <Foundation/Foundation.h>
#include <mach/mach.h>
#include "base/apple/scoped_mach_port.h"
#include "base/numerics/safe_conversions.h"

namespace kiwi::base {

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  NSProcessInfo* process_info = [NSProcessInfo processInfo];
  meminfo->total = static_cast<int>(process_info.physicalMemory / 1024);

  base::apple::ScopedMachSendRight host(mach_host_self());
  vm_statistics64_data_t vm_info;
  mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
  if (host_statistics64(host.get(), HOST_VM_INFO64,
                        reinterpret_cast<host_info64_t>(&vm_info),
                        &count) != KERN_SUCCESS) {
    return false;
  }
  DCHECK_EQ(HOST_VM_INFO64_COUNT, count);

#if !(BUILDFLAG(IS_IOS) && defined(ARCH_CPU_X86_FAMILY))
  // PAGE_SIZE (aka vm_page_size) isn't constexpr, so this check needs to be
  // done at runtime.
  DCHECK_EQ(PAGE_SIZE % 1024, 0u) << "Invalid page size";
#else
  // On x86/x64, PAGE_SIZE used to be just a signed constant, I386_PGBYTES. When
  // Arm Macs started shipping, PAGE_SIZE was defined from day one to be
  // vm_page_size (an extern uintptr_t value), and the SDK, for x64, switched
  // PAGE_SIZE to be vm_page_size for binaries targeted for macOS 11+:
  //
  // #if !defined(__MAC_OS_X_VERSION_MIN_REQUIRED) ||
  //     (__MAC_OS_X_VERSION_MIN_REQUIRED < 101600)
  //   #define PAGE_SIZE    I386_PGBYTES
  // #else
  //   #define PAGE_SIZE    vm_page_size
  // #endif
  //
  // When building for Mac Catalyst or the iOS Simulator, this targeting
  // switcharoo breaks. Because those apps do not have a
  // __MAC_OS_X_VERSION_MIN_REQUIRED set, the SDK assumes that those apps are so
  // old that they are implicitly targeting some ancient version of macOS, and a
  // signed constant value is used for PAGE_SIZE.
  //
  // Therefore, when building for "iOS on x86", which is either Mac Catalyst or
  // the iOS Simulator, use a static assert that assumes that PAGE_SIZE is a
  // signed constant value.
  //
  // TODO(Chrome iOS team): Remove this entire #else branch when the Mac
  // Catalyst and the iOS Simulator builds only target Arm Macs.
  static_assert(PAGE_SIZE % 1024 == 0, "Invalid page size");
#endif  // !(defined(IS_IOS) && defined(ARCH_CPU_X86_FAMILY))

  if (vm_info.speculative_count <= vm_info.free_count) {
    meminfo->free = saturated_cast<int>(
        PAGE_SIZE / 1024 * (vm_info.free_count - vm_info.speculative_count));
  } else {
    // Inside the `host_statistics64` call above, `speculative_count` is
    // computed later than `free_count`, so these values are snapshots of two
    // (slightly) different points in time. As a result, it is possible for
    // `speculative_count` to have increased significantly since `free_count`
    // was computed, even to a point where `speculative_count` is greater than
    // the computed value of `free_count`. See
    // https://github.com/apple-oss-distributions/xnu/blob/aca3beaa3dfbd42498b42c5e5ce20a938e6554e5/osfmk/kern/host.c#L788
    // In this case, 0 is the best approximation for `meminfo->free`. This is
    // inexact, but even in the case where `speculative_count` is less than
    // `free_count`, the computed `meminfo->free` will only be an approximation
    // given that the two inputs come from different points in time.
    meminfo->free = 0;
  }

  meminfo->speculative =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.speculative_count);
  meminfo->file_backed =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.external_page_count);
  meminfo->purgeable =
      saturated_cast<int>(PAGE_SIZE / 1024 * vm_info.purgeable_count);

  return true;
}

}  // namespace kiwi::base