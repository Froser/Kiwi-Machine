// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file_util.h"

#include <unistd.h>
#include <stack>

#include "base/containers/adapters.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"

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

}  // namespace kiwi::base
