// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_LOCK_H_
#define BASE_SYNCHRONIZATION_LOCK_H_

#include "base/base_export.h"
#include "base/dcheck_is_on.h"
#include "base/synchronization/lock_impl.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

namespace kiwi::base {

// A convenient wrapper for an OS specific critical section.  The only real
// intelligence in this class is in debug mode for the support for the
// AssertAcquired() method.
class LOCKABLE BASE_EXPORT Lock {
 public:
  // Optimized wrapper implementation
  Lock() : lock_() {}

  Lock(const Lock&) = delete;
  Lock& operator=(const Lock&) = delete;

  ~Lock() {}

  void Acquire() EXCLUSIVE_LOCK_FUNCTION() { lock_.Lock(); }
  void Release() UNLOCK_FUNCTION() { lock_.Unlock(); }

  // If the lock is not held, take it and return true. If the lock is already
  // held by another thread, immediately return false. This must not be called
  // by a thread already holding the lock (what happens is undefined and an
  // assertion may fail).
  bool Try() EXCLUSIVE_TRYLOCK_FUNCTION(true) { return lock_.Try(); }

  // Null implementation if not debug.
  void AssertAcquired() const ASSERT_EXCLUSIVE_LOCK() {}
  void AssertNotHeld() const {}

  // Whether Lock mitigates priority inversion when used from different thread
  // priorities.
  static bool HandlesMultipleThreadPriorities() {
#if BUILDFLAG(IS_WIN)
    // Windows mitigates priority inversion by randomly boosting the priority of
    // ready threads.
    // https://msdn.microsoft.com/library/windows/desktop/ms684831.aspx
    return true;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    // POSIX mitigates priority inversion by setting the priority of a thread
    // holding a Lock to the maximum priority of any other thread waiting on it.
    return internal::LockImpl::PriorityInheritanceAvailable();
#else
#error Unsupported platform
#endif
  }

  // Both Windows and POSIX implementations of ConditionVariable need to be
  // able to see our lock and tweak our debugging counters, as they release and
  // acquire locks inside of their condition variable APIs.
  friend class ConditionVariable;

 private:
  // Platform specific underlying lock implementation.
  kiwi::base::internal::LockImpl lock_;
};

// A helper class that acquires the given Lock while the AutoLock is in scope.
using AutoLock = internal::BasicAutoLock<Lock>;

// A helper class that tries to acquire the given Lock while the AutoTryLock is
// in scope.
using AutoTryLock = internal::BasicAutoTryLock<Lock>;

// AutoUnlock is a helper that will Release() the |lock| argument in the
// constructor, and re-Acquire() it in the destructor.
using AutoUnlock = internal::BasicAutoUnlock<Lock>;

// Like AutoLock but is a no-op when the provided Lock* is null. Inspired from
// absl::MutexLockMaybe. Use this instead of absl::optional<base::AutoLock> to
// get around -Wthread-safety-analysis warnings for conditional locking.
using AutoLockMaybe = internal::BasicAutoLockMaybe<Lock>;

// Like AutoLock but permits Release() of its mutex before destruction.
// Release() may be called at most once. Inspired from
// absl::ReleasableMutexLock. Use this instead of absl::optional<base::AutoLock>
// to get around -Wthread-safety-analysis warnings for AutoLocks that are
// explicitly released early (prefer proper scoping to this).
using ReleasableAutoLock = internal::BasicReleasableAutoLock<Lock>;

}  // namespace base

#endif  // BASE_SYNCHRONIZATION_LOCK_H_
