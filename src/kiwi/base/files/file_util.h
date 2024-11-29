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

#ifndef BASE_FILES_FILE_UTIL_H_
#define BASE_FILES_FILE_UTIL_H_

#include <stdint.h>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

#include "base/base_export.h"
#include "base/files/file.h"

#if defined(CreateDirectory)
#undef CreateDirectory
#endif

namespace kiwi::base {

class FilePath;

// Deletes the given path, whether it's a file or a directory.
// If it's a directory, it's perfectly happy to delete all of the
// directory's contents, including subdirectories and their contents.
// Returns true if successful, false otherwise. It is considered successful
// to attempt to delete a file that does not exist.
//
// In POSIX environment and if |path| is a symbolic link, this deletes only
// the symlink. (even if the symlink points to a non-existent file)
//
// WARNING: USING THIS EQUIVALENT TO "rm -rf", SO USE WITH CAUTION.
BASE_EXPORT bool DeletePathRecursively(const FilePath& path);

// Creates a directory, as well as creating any parent directories, if they
// don't exist. Returns 'true' on successful creation, or if the directory
// already exists.  The directory is only readable by the current user.
// Returns true on success, leaving *error unchanged.
// Returns false on failure and sets *error appropriately, if it is non-NULL.
BASE_EXPORT bool CreateDirectoryAndGetError(const FilePath& full_path,
                                            File::Error* error);

// Backward-compatible convenience method for the above.
BASE_EXPORT bool CreateDirectory(const FilePath& full_path);

// Returns true if the given path exists on the local filesystem,
// false otherwise.
BASE_EXPORT bool PathExists(const FilePath& path);

// Returns true if the given path exists and is a directory, false otherwise.
BASE_EXPORT bool DirectoryExists(const FilePath& path);

// Wrapper for fopen-like calls. Returns non-NULL FILE* on success. The
// underlying file descriptor (POSIX) or handle (Windows) is unconditionally
// configured to not be propagated to child processes.
BASE_EXPORT FILE* OpenFile(const FilePath& filename, const char* mode);

// Closes file opened by OpenFile. Returns true on success.
BASE_EXPORT bool CloseFile(FILE* file);

// Reads the file at |path| and returns a vector of bytes on success, and
// nullopt on error. For security reasons, a |path| containing path traversal
// components ('..') is treated as a read error, returning nullopt.
BASE_EXPORT std::optional<std::vector<uint8_t>> ReadFileToBytes(
    const FilePath& path);

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
// Sets the given |fd| to close-on-exec mode.
// Returns true if it was able to set it in the close-on-exec mode, otherwise
// false.
BASE_EXPORT bool SetCloseOnExec(int fd);
#endif

// Copies a single file. Use CopyDirectory() to copy directories.
// This function fails if either path contains traversal components ('..').
// This function also fails if |to_path| is a directory.
//
// On POSIX, if |to_path| is a symlink, CopyFile() will follow the symlink. This
// may have security implications. Use with care.
//
// If |to_path| already exists and is a regular file, it will be overwritten,
// though its permissions will stay the same.
//
// If |to_path| does not exist, it will be created. The new file's permissions
// varies per platform:
//
// - This function keeps the metadata on Windows. The read only bit is not kept.
// - On Mac and iOS, |to_path| retains |from_path|'s permissions, except user
//   read/write permissions are always set.
// - On Linux and Android, |to_path| has user read/write permissions only. i.e.
//   Always 0600.
// - On ChromeOS, |to_path| has user read/write permissions and group/others
//   read permissions. i.e. Always 0644.
BASE_EXPORT bool CopyFile(const FilePath& from_path, const FilePath& to_path);

}  // namespace kiwi::base

#endif  // BASE_FILES_FILE_UTIL_H_
