// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file_util.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stack>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"

namespace kiwi::base {

namespace {

bool CopyFileContents(int infile, int outfile) {
  static constexpr size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);

  for (;;) {
    int bytes_read =
        read(infile, buffer.data(), static_cast<int>(buffer.size()));
    if (bytes_read < 0) {
      return false;
    }
    if (bytes_read == 0) {
      return true;
    }
    // Allow for partial writes
    int bytes_written_per_read = 0;
    do {
      int bytes_written_partial =
          write(outfile, &buffer[static_cast<size_t>(bytes_written_per_read)],
                bytes_read - bytes_written_per_read);
      if (bytes_written_partial < 0) {
        return false;
      }

      bytes_written_per_read += bytes_written_partial;
    } while (bytes_written_per_read < bytes_read);
  }

  NOTREACHED();
  return false;
}

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

bool AdvanceEnumeratorWithStat(FileEnumerator* traversal,
                               FilePath* out_next_path,
                               stat_wrapper_t* out_next_stat) {
  DCHECK(out_next_path);
  DCHECK(out_next_stat);
  *out_next_path = traversal->Next();
  if (out_next_path->empty())
    return false;

  *out_next_stat = traversal->GetInfo().stat();
  return true;
}

bool DoCopyDirectory(const FilePath& from_path,
                     const FilePath& to_path,
                     bool recursive,
                     bool open_exclusive) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE,
  // BlockingType::MAY_BLOCK); Some old callers of CopyDirectory want it to
  // support wildcards. After some discussion, we decided to fix those callers.
  // Break loudly here if anyone tries to do this.
  DCHECK(to_path.value().find('*') == std::string::npos);
  DCHECK(from_path.value().find('*') == std::string::npos);

  if (from_path.value().size() >= PATH_MAX) {
    return false;
  }

  // This function does not properly handle destinations within the source
  FilePath real_to_path = to_path;
  if (PathExists(real_to_path))
    real_to_path = MakeAbsoluteFilePath(real_to_path);
  else
    real_to_path = MakeAbsoluteFilePath(real_to_path.DirName());
  if (real_to_path.empty())
    return false;

  FilePath real_from_path = MakeAbsoluteFilePath(from_path);
  if (real_from_path.empty())
    return false;
  if (real_to_path == real_from_path || real_from_path.IsParent(real_to_path))
    return false;

  int traverse_type = FileEnumerator::FILES | FileEnumerator::SHOW_SYM_LINKS;
  if (recursive)
    traverse_type |= FileEnumerator::DIRECTORIES;
  FileEnumerator traversal(from_path, recursive, traverse_type);

  // We have to mimic windows behavior here. |to_path| may not exist yet,
  // start the loop with |to_path|.
  stat_wrapper_t from_stat;
  FilePath current = from_path;
  if (File::Stat(from_path.value().c_str(), &from_stat) < 0) {
    DLOG(ERROR) << "CopyDirectory() couldn't stat source directory: "
                << from_path.value();
    return false;
  }
  FilePath from_path_base = from_path;
  if (recursive && DirectoryExists(to_path)) {
    // If the destination already exists and is a directory, then the
    // top level of source needs to be copied.
    from_path_base = from_path.DirName();
  }

  // The Windows version of this function assumes that non-recursive calls
  // will always have a directory for from_path.
  // TODO(maruel): This is not necessary anymore.
  DCHECK(recursive || S_ISDIR(from_stat.st_mode));

  do {
    // current is the source path, including from_path, so append
    // the suffix after from_path to to_path to create the target_path.
    FilePath target_path(to_path);
    if (from_path_base != current &&
        !from_path_base.AppendRelativePath(current, &target_path)) {
      return false;
    }

    if (S_ISDIR(from_stat.st_mode)) {
      mode_t mode = (from_stat.st_mode & 01777) | S_IRUSR | S_IXUSR | S_IWUSR;
      if (mkdir(target_path.value().c_str(), mode) == 0)
        continue;
      if (errno == EEXIST && !open_exclusive)
        continue;

      DLOG(ERROR) << "CopyDirectory() couldn't create directory: "
                  << target_path.value();
      return false;
    }

    if (!S_ISREG(from_stat.st_mode)) {
      DLOG(WARNING) << "CopyDirectory() skipping non-regular file: "
                    << current.value();
      continue;
    }

    // Add O_NONBLOCK so we can't block opening a pipe.
    ScopedFD infile(open(current.value().c_str(), O_RDONLY | O_NONBLOCK));
    if (!infile.is_valid()) {
      DLOG(ERROR) << "CopyDirectory() couldn't open file: " << current.value();
      return false;
    }

    stat_wrapper_t stat_at_use;
    if (File::Fstat(infile.get(), &stat_at_use) < 0) {
      DLOG(ERROR) << "CopyDirectory() couldn't stat file: " << current.value();
      return false;
    }

    if (!S_ISREG(stat_at_use.st_mode)) {
      DLOG(WARNING) << "CopyDirectory() skipping non-regular file: "
                    << current.value();
      continue;
    }

    int open_flags = O_WRONLY | O_CREAT;
    // If |open_exclusive| is set then we should always create the destination
    // file, so O_NONBLOCK is not necessary to ensure we don't block on the
    // open call for the target file below, and since the destination will
    // always be a regular file it wouldn't affect the behavior of the
    // subsequent write calls anyway.
    if (open_exclusive)
      open_flags |= O_EXCL;
    else
      open_flags |= O_TRUNC | O_NONBLOCK;
      // Each platform has different default file opening modes for CopyFile
      // which we want to replicate here. On OS X, we use copyfile(3) which
      // takes the source file's permissions into account. On the other
      // platforms, we just use the base::File constructor. On Chrome OS,
      // base::File uses a different set of permissions than it does on other
      // POSIX platforms.
#if BUILDFLAG(IS_APPLE)
    mode_t mode = 0600 | (stat_at_use.st_mode & 0177);
#elif BUILDFLAG(IS_CHROMEOS)
    mode_t mode = 0644;
#else
    mode_t mode = 0600;
#endif
    ScopedFD outfile(open(target_path.value().c_str(), open_flags, mode));
    if (!outfile.is_valid()) {
      DLOG(ERROR) << "CopyDirectory() couldn't create file: "
                  << target_path.value();
      return false;
    }

    if (!CopyFileContents(infile.get(), outfile.get())) {
      DLOG(ERROR) << "CopyDirectory() couldn't copy file: " << current.value();
      return false;
    }
  } while (AdvanceEnumeratorWithStat(&traversal, &current, &from_stat));

  return true;
}

}  // namespace

FilePath MakeAbsoluteFilePath(const FilePath& input) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE,
  // BlockingType::MAY_BLOCK);
  char full_path[PATH_MAX];
  if (realpath(input.value().c_str(), full_path) == nullptr)
    return FilePath();
  return FilePath(full_path);
}

bool DeleteFile(const FilePath& path) {
  return DoDeleteFile(path, /*recursive=*/false);
}

bool DeletePathRecursively(const FilePath& path) {
  return DoDeleteFile(path, /*recursive=*/true);
}

bool CopyDirectory(const FilePath& from_path,
                   const FilePath& to_path,
                   bool recursive) {
  return DoCopyDirectory(from_path, to_path, recursive, false);
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
