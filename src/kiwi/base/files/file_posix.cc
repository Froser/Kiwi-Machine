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

void File::Info::FromStat(const stat_wrapper_t& stat_info) {
  is_directory = S_ISDIR(stat_info.st_mode);
  is_symbolic_link = S_ISLNK(stat_info.st_mode);
  size = stat_info.st_size;

  // We don't support Time class yet.

  /*
  // Get last modification time, last access time, and creation time from
  // |stat_info|.
  // Note: st_ctime is actually last status change time when the inode was last
  // updated, which happens on any metadata change. It is not the file's
  // creation time. However, other than on Mac & iOS where the actual file
  // creation time is included as st_birthtime, the rest of POSIX platforms have
  // no portable way to get the creation time.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  time_t last_modified_sec = stat_info.st_mtim.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtim.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atim.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atim.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctim.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctim.tv_nsec;
#elif BUILDFLAG(IS_ANDROID)
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = stat_info.st_mtime_nsec;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = stat_info.st_atime_nsec;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = stat_info.st_ctime_nsec;
#elif BUILDFLAG(IS_APPLE)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_birthtimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_birthtimespec.tv_nsec;
#elif BUILDFLAG(IS_BSD)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctimespec.tv_nsec;
#else
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = 0;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = 0;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = 0;
#endif

  last_modified =
      Time::FromTimeT(last_modified_sec) +
      Microseconds(last_modified_nsec / Time::kNanosecondsPerMicrosecond);

  last_accessed =
      Time::FromTimeT(last_accessed_sec) +
      Microseconds(last_accessed_nsec / Time::kNanosecondsPerMicrosecond);

  creation_time =
      Time::FromTimeT(creation_time_sec) +
      Microseconds(creation_time_nsec / Time::kNanosecondsPerMicrosecond);
      */
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
