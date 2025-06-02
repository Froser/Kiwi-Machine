// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/apple/scoped_nsautorelease_pool.h"

#include "base/dcheck_is_on.h"

#if DCHECK_IS_ON()
#import <Foundation/Foundation.h>

#include "base/debug/stack_trace.h"
#include "base/immediate_crash.h"
#include "base/strings/sys_string_conversions.h"
#endif

// Note that this uses the direct runtime interface to the autorelease pool.
// https://clang.llvm.org/docs/AutomaticReferenceCounting.html#runtime-support
// This is so this can work when compiled for ARC.
extern "C" {
void* objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void* pool);
}

namespace kiwi::base::apple {

ScopedNSAutoreleasePool::ScopedNSAutoreleasePool() {
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PushImpl();
}

ScopedNSAutoreleasePool::~ScopedNSAutoreleasePool() {
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  PopImpl();
}

void ScopedNSAutoreleasePool::Recycle() {
  // DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Cycle the internal pool, allowing everything there to get cleaned up and
  // start anew.
  PopImpl();
  PushImpl();
}

void ScopedNSAutoreleasePool::PushImpl() {
  autorelease_pool_ = objc_autoreleasePoolPush();
}

void ScopedNSAutoreleasePool::PopImpl() {
  objc_autoreleasePoolPop(autorelease_pool_);
}

}  // namespace base::apple
