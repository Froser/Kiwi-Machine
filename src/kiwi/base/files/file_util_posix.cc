// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file_util.h"

#include <fcntl.h>
#include <unistd.h>
#include <array>
#include <stack>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"

namespace kiwi::base {

namespace {
// TODO(erikkay): The Windows version of this accepts paths like "foo/bar/*"
// which works both with and without the recursive flag.  I'm not sure we need
// that functionality. If not, remove from file_util_win.cc, otherwise add it
// here.
bool DoDeleteFile(const FilePath& path, bool recursive) {
  const char* path_str = path.value().c_str();
  stat_wrapper_t file_info;
  if (File::Lstat(path_str, &file_info) != 0) {
    // The Windows version defines this condition as success.
    return (errno == ENOENT);
  }
  if (!S_ISDIR(file_info.st_mode))
    return (unlink(path_str) == 0) || (errno == ENOENT);
  if (!recursive)
    return (rmdir(path_str) == 0) || (errno == ENOENT);

  bool success = true;
  std::stack<std::string> directories;
  directories.push(path.value());
  FileEnumerator traversal(path, true,
                           FileEnumerator::FILES | FileEnumerator::DIRECTORIES |
                               FileEnumerator::SHOW_SYM_LINKS);
  for (FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    if (traversal.GetInfo().IsDirectory())
      directories.push(current.value());
    else
      success &= (unlink(current.value().c_str()) == 0) || (errno == ENOENT);
  }

  while (!directories.empty()) {
    FilePath dir = FilePath(directories.top());
    directories.pop();
    success &= (rmdir(dir.value().c_str()) == 0) || (errno == ENOENT);
  }
  return success;
}

#if !BUILDFLAG(IS_APPLE)
// Appends |mode_char| to |mode| before the optional character set encoding; see
// https://www.gnu.org/software/libc/manual/html_node/Opening-Streams.html for
// details.
std::string AppendModeCharacter(StringPiece mode, char mode_char) {
  std::string result(mode);
  size_t comma_pos = result.find(',');
  result.insert(comma_pos == std::string::npos ? result.length() : comma_pos, 1,
                mode_char);
  return result;
}
#endif

}  // namespace

bool DeletePathRecursively(const FilePath& path) {
  return DoDeleteFile(path, /*recursive=*/true);
}

bool PathExists(const FilePath& path) {
  return access(path.value().c_str(), F_OK) == 0;
}

bool DirectoryExists(const FilePath& path) {
  stat_wrapper_t file_info;
  if (File::Stat(path.value().c_str(), &file_info) != 0)
    return false;
  return S_ISDIR(file_info.st_mode);
}

bool CreateDirectoryAndGetError(const FilePath& full_path, File::Error* error) {
  std::vector<FilePath> subpaths;

  // Collect a list of all parent directories.
  FilePath last_path = full_path;
  subpaths.push_back(full_path);
  for (FilePath path = full_path.DirName(); path.value() != last_path.value();
       path = path.DirName()) {
    subpaths.push_back(path);
    last_path = path;
  }

  // Iterate through the parents and create the missing ones.
  for (const FilePath& subpath : base::Reversed(subpaths)) {
    if (DirectoryExists(subpath))
      continue;
    if (mkdir(subpath.value().c_str(), 0700) == 0)
      continue;
    // Mkdir failed, but it might have failed with EEXIST, or some other error
    // due to the directory appearing out of thin air. This can occur if
    // two processes are trying to create the same file system tree at the same
    // time. Check to see if it exists and make sure it is a directory.
    int saved_errno = errno;
    if (!DirectoryExists(subpath)) {
      if (error)
        *error = File::OSErrorToFileError(saved_errno);
      return false;
    }
  }
  return true;
}

bool GetFileInfo(const FilePath& file_path, File::Info* results) {
  stat_wrapper_t file_info;
  if (File::Stat(file_path.value().c_str(), &file_info) != 0) {
    return false;
  }

  results->FromStat(file_info);
  return true;
}

#if !BUILDFLAG(IS_APPLE)
bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  if (from_path.ReferencesParent() || to_path.ReferencesParent()) {
    return false;
  }

  const int source_fd =
      HANDLE_EINTR(open(from_path.value().c_str(), O_RDONLY | O_NONBLOCK));
  if (source_fd < 0) {
    return false;
  }

  stat_wrapper_t source_info;
  if (File::Fstat(source_fd, &source_info) != 0 ||
      !S_ISREG(source_info.st_mode)) {
    IGNORE_EINTR(close(source_fd));
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  constexpr mode_t kDestinationMode = 0644;
#else
  constexpr mode_t kDestinationMode = 0600;
#endif
  const int destination_fd =
      HANDLE_EINTR(open(to_path.value().c_str(),
                        O_WRONLY | O_CREAT | O_NONBLOCK, kDestinationMode));
  if (destination_fd < 0) {
    IGNORE_EINTR(close(source_fd));
    return false;
  }

  stat_wrapper_t destination_info;
  const bool valid_destination =
      File::Fstat(destination_fd, &destination_info) == 0 &&
      S_ISREG(destination_info.st_mode) &&
      (source_info.st_dev != destination_info.st_dev ||
       source_info.st_ino != destination_info.st_ino);
  if (!valid_destination || HANDLE_EINTR(ftruncate(destination_fd, 0)) != 0) {
    IGNORE_EINTR(close(destination_fd));
    IGNORE_EINTR(close(source_fd));
    return false;
  }

  std::array<uint8_t, 32 * 1024> buffer;
  bool success = true;
  while (true) {
    const ssize_t bytes_read =
        HANDLE_EINTR(read(source_fd, buffer.data(), buffer.size()));
    if (bytes_read == 0) {
      break;
    }
    if (bytes_read < 0 ||
        !WriteFileDescriptor(
            destination_fd,
            std::span(buffer.data(), static_cast<size_t>(bytes_read)))) {
      success = false;
      break;
    }
  }

  success = IGNORE_EINTR(close(destination_fd)) == 0 && success;
  IGNORE_EINTR(close(source_fd));
  return success;
}
#endif

FILE* OpenFile(const FilePath& filename, const char* mode) {
  // 'e' is unconditionally added below, so be sure there is not one already
  // present before a comma in |mode|.
  DCHECK(
      strchr(mode, 'e') == nullptr ||
      (strchr(mode, ',') != nullptr && strchr(mode, 'e') > strchr(mode, ',')));
  // Do not check blocking call because it is not supported.
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE,
  // BlockingType::MAY_BLOCK);
  FILE* result = nullptr;
#if BUILDFLAG(IS_APPLE)
  // macOS does not provide a mode character to set O_CLOEXEC; see
  // https://developer.apple.com/legacy/library/documentation/Darwin/Reference/ManPages/man3/fopen.3.html.
  const char* the_mode = mode;
#else
  std::string mode_with_e(AppendModeCharacter(mode, 'e'));
  const char* the_mode = mode_with_e.c_str();
#endif
  do {
    result = fopen(filename.value().c_str(), the_mode);
  } while (!result && errno == EINTR);
#if BUILDFLAG(IS_APPLE)
  // Mark the descriptor as close-on-exec.
  if (result) {
    SetCloseOnExec(fileno(result));
  }
#endif
  return result;
}

int WriteFile(const FilePath& filename, const char* data, int size) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE,
  // BlockingType::MAY_BLOCK);
  if (size < 0)
    return -1;
  int fd = HANDLE_EINTR(creat(filename.value().c_str(), 0666));
  if (fd < 0)
    return -1;

  int bytes_written =
      WriteFileDescriptor(fd, StringPiece(data, static_cast<size_t>(size)))
          ? size
          : -1;
  if (IGNORE_EINTR(close(fd)) < 0)
    return -1;
  return bytes_written;
}

bool WriteFileDescriptor(int fd, std::span<const uint8_t> data) {
  // Allow for partial writes.
  ssize_t bytes_written_total = 0;
  ssize_t size = checked_cast<ssize_t>(data.size());
  for (ssize_t bytes_written_partial = 0; bytes_written_total < size;
       bytes_written_total += bytes_written_partial) {
    bytes_written_partial =
        HANDLE_EINTR(write(fd, data.data() + bytes_written_total,
                           static_cast<size_t>(size - bytes_written_total)));
    if (bytes_written_partial < 0)
      return false;
  }

  return true;
}

bool WriteFileDescriptor(int fd, StringPiece data) {
  return WriteFileDescriptor(
      fd,
      std::span(reinterpret_cast<const uint8_t*>(data.data()), data.size()));
}

bool SetCloseOnExec(int fd) {
  const int flags = fcntl(fd, F_GETFD);
  if (flags == -1) {
    return false;
  }
  if (flags & FD_CLOEXEC) {
    return true;
  }
  if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1) {
    return false;
  }
  return true;
}

}  // namespace kiwi::base
