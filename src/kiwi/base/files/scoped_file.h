// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_SCOPED_FILE_H_
#define BASE_FILES_SCOPED_FILE_H_

#include <stdio.h>

#include <memory>

#include "base/base_export.h"
#include "base/scoped_generic.h"
#include "build/build_config.h"

namespace kiwi::base {

namespace internal {

#if BUILDFLAG(IS_ANDROID)
// Use fdsan on android.
struct BASE_EXPORT ScopedFDCloseTraits : public ScopedGenericOwnershipTracking {
  static int InvalidValue() { return -1; }
  static void Free(int);
  static void Acquire(const ScopedGeneric<int, ScopedFDCloseTraits>&, int);
  static void Release(const ScopedGeneric<int, ScopedFDCloseTraits>&, int);
};
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
// On ChromeOS and Linux we guard FD lifetime with a global table and hook into
// libc close() to perform checks.
struct BASE_EXPORT ScopedFDCloseTraits : public ScopedGenericOwnershipTracking {
#else
struct BASE_EXPORT ScopedFDCloseTraits {
#endif
  static int InvalidValue() {
    return -1;
  }
  static void Free(int fd);
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  static void Acquire(const ScopedGeneric<int, ScopedFDCloseTraits>&, int);
  static void Release(const ScopedGeneric<int, ScopedFDCloseTraits>&, int);
#endif
};
#endif

// Functor for |ScopedFILE| (below).
struct ScopedFILECloser {
  inline void operator()(FILE* x) const {
    if (x)
      fclose(x);
  }
};

}  // namespace internal

// -----------------------------------------------------------------------------

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// A low-level Posix file descriptor closer class. Use this when writing
// platform-specific code, especially that does non-file-like things with the
// FD (like sockets).
//
// If you're writing low-level Windows code, see base/win/scoped_handle.h
// which provides some additional functionality.
//
// If you're writing cross-platform code that deals with actual files, you
// should generally use base::File instead which can be constructed with a
// handle, and in addition to handling ownership, has convenient cross-platform
// file manipulation functions on it.
typedef ScopedGeneric<int, internal::ScopedFDCloseTraits> ScopedFD;
#endif

// Automatically closes |FILE*|s.
typedef std::unique_ptr<FILE, internal::ScopedFILECloser> ScopedFILE;

}  // namespace kiwi::base

#endif  // BASE_FILES_SCOPED_FILE_H_
