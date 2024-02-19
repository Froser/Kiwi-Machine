// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file.h"

#include <sys/stat.h>

#include "base/check.h"

namespace kiwi::base {

// Static.
File::Error File::OSErrorToFileError(int saved_errno) {
  switch (saved_errno) {
    case EACCES:
    case EISDIR:
    case EROFS:
    case EPERM:
      return FILE_ERROR_ACCESS_DENIED;
    case EBUSY:
    case ETXTBSY:
      return FILE_ERROR_IN_USE;
    case EEXIST:
      return FILE_ERROR_EXISTS;
    case EIO:
      return FILE_ERROR_IO;
    case ENOENT:
      return FILE_ERROR_NOT_FOUND;
    case ENFILE:  // fallthrough
    case EMFILE:
      return FILE_ERROR_TOO_MANY_OPENED;
    case ENOMEM:
      return FILE_ERROR_NO_MEMORY;
    case ENOSPC:
      return FILE_ERROR_NO_SPACE;
    case ENOTDIR:
      return FILE_ERROR_NOT_A_DIRECTORY;
    default:
      // This function should only be called for errors.
      DCHECK_NE(0, saved_errno);
      return FILE_ERROR_FAILED;
  }
}

#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_NACL) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)
int File::Stat(const char* path, stat_wrapper_t* sb) {
  return stat(path, sb);
}
int File::Fstat(int fd, stat_wrapper_t* sb) {
  return fstat(fd, sb);
}
int File::Lstat(const char* path, stat_wrapper_t* sb) {
  return lstat(path, sb);
}
#else

#if BUILDFLAG(IS_WASM)

int File::Stat(const char* path, stat_wrapper_t* sb) {
  return stat(path, sb);
}
int File::Fstat(int fd, stat_wrapper_t* sb) {
  return fstat(fd, sb);
}
int File::Lstat(const char* path, stat_wrapper_t* sb) {
  return lstat(path, sb);
}

#else

int File::Stat(const char* path, stat_wrapper_t* sb) {
  return stat64(path, sb);
}
int File::Fstat(int fd, stat_wrapper_t* sb) {
  return fstat64(fd, sb);
}
int File::Lstat(const char* path, stat_wrapper_t* sb) {
  return lstat64(path, sb);
}

#endif

#endif

}  // namespace kiwi::base
