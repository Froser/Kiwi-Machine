// Copyright (C) 2023 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#ifndef BASE_FILES_FILE_H_
#define BASE_FILES_FILE_H_

#include <stdint.h>
#include <fstream>
#include <memory>

#include "base/base_export.h"
#include "base/platform/platform_factory.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

#if BUILDFLAG(IS_BSD) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_NACL) || \
    BUILDFLAG(IS_FUCHSIA) || (BUILDFLAG(IS_ANDROID))
struct stat;
namespace kiwi::base {
typedef struct stat stat_wrapper_t;
}
#elif BUILDFLAG(IS_POSIX)
struct stat64;
namespace base {
typedef struct stat64 stat_wrapper_t;
}
#endif

namespace kiwi::base {
class FilePath;
class BASE_EXPORT File {
 public:
  enum Flags {
    FLAG_OPEN = 1 << 0,            // Opens a file, only if it exists.
    FLAG_CREATE = 1 << 1,          // Creates a new file, only if it does not
                                   // already exist.
    FLAG_OPEN_ALWAYS = 1 << 2,     // May create a new file.
    FLAG_CREATE_ALWAYS = 1 << 3,   // May overwrite an old file.
    FLAG_OPEN_TRUNCATED = 1 << 4,  // Opens a file and truncates it, only if it
                                   // exists.
    FLAG_READ = 1 << 5,
    FLAG_WRITE = 1 << 6,
    FLAG_APPEND = 1 << 7,
  };

  // This enum has been recorded in multiple histograms using PlatformFileError
  // enum. If the order of the fields needs to change, please ensure that those
  // histograms are obsolete or have been moved to a different enum.
  //
  // FILE_ERROR_ACCESS_DENIED is returned when a call fails because of a
  // filesystem restriction. FILE_ERROR_SECURITY is returned when a browser
  // policy doesn't allow the operation to be executed.
  enum Error {
    FILE_OK = 0,
    FILE_ERROR_FAILED = -1,
    FILE_ERROR_IN_USE = -2,
    FILE_ERROR_EXISTS = -3,
    FILE_ERROR_NOT_FOUND = -4,
    FILE_ERROR_ACCESS_DENIED = -5,
    FILE_ERROR_TOO_MANY_OPENED = -6,
    FILE_ERROR_NO_MEMORY = -7,
    FILE_ERROR_NO_SPACE = -8,
    FILE_ERROR_NOT_A_DIRECTORY = -9,
    FILE_ERROR_INVALID_OPERATION = -10,
    FILE_ERROR_SECURITY = -11,
    FILE_ERROR_ABORT = -12,
    FILE_ERROR_NOT_A_FILE = -13,
    FILE_ERROR_NOT_EMPTY = -14,
    FILE_ERROR_INVALID_URL = -15,
    FILE_ERROR_IO = -16,
    // Put new entries here and increment FILE_ERROR_MAX.
    FILE_ERROR_MAX = -17
  };

  // This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.
  enum Whence { FROM_BEGIN = 0, FROM_CURRENT = 1, FROM_END = 2 };

  File(const FilePath& path, uint32_t flags);
  ~File();

 public:
  void Open(const FilePath& file_path, uint32_t flags);
  int64_t GetLength();
  int Read(int64_t offset, char* data, int size);
  int ReadAtCurrentPos(char* data, int size);
  int Write(int64_t offset, const char* data, int size);
  int WriteAtCurrentPos(const char* data, int size);

  // Changes current position in the file to an |offset| relative to an origin
  // defined by |whence|. Returns the resultant current position in the file
  // (relative to the start) or -1 in case of error.
  int64_t Seek(Whence whence, int64_t offset);

  bool IsValid() const;

#if BUILDFLAG(IS_WIN)
  static Error OSErrorToFileError(DWORD last_error);
#elif BUILDFLAG(IS_POSIX)
  static Error OSErrorToFileError(int saved_errno);
#endif

#if BUILDFLAG(IS_POSIX)
  // Wrapper for stat() or stat64().
  static int Stat(const char* path, stat_wrapper_t* sb);
  static int Fstat(int fd, stat_wrapper_t* sb);
  static int Lstat(const char* path, stat_wrapper_t* sb);
#endif

 private:
  std::fstream file_;
};
}  // namespace kiwi::base

#endif  // BASE_FILES_FILE_H_
