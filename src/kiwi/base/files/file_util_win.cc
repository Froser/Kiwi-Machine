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

#include "base/files/file_util.h"

#include <windows.h>

#include "base/check.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"

namespace kiwi::base {

namespace {

// Returns the Win32 last error code or ERROR_SUCCESS if the last error code is
// ERROR_FILE_NOT_FOUND or ERROR_PATH_NOT_FOUND. This is useful in cases where
// the absence of a file or path is a success condition (e.g., when attempting
// to delete an item in the filesystem).
DWORD ReturnLastErrorOrSuccessOnNotFound() {
  const DWORD error_code = ::GetLastError();
  return (error_code == ERROR_FILE_NOT_FOUND ||
          error_code == ERROR_PATH_NOT_FOUND)
             ? ERROR_SUCCESS
             : error_code;
}

// Deletes all files and directories in a path.
// Returns ERROR_SUCCESS on success or the Windows error code corresponding to
// the first error encountered. ERROR_FILE_NOT_FOUND and ERROR_PATH_NOT_FOUND
// are considered success conditions, and are therefore never returned.
DWORD DeleteFileRecursive(const FilePath& path,
                          const FilePath::StringType& pattern,
                          bool recursive) {
  FileEnumerator traversal(path, false,
                           FileEnumerator::FILES | FileEnumerator::DIRECTORIES,
                           pattern);
  DWORD result = ERROR_SUCCESS;
  for (FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    // Try to clear the read-only bit if we find it.
    FileEnumerator::FileInfo info = traversal.GetInfo();
    if ((info.find_data().dwFileAttributes & FILE_ATTRIBUTE_READONLY) &&
        (recursive || !info.IsDirectory())) {
      ::SetFileAttributes(
          current.value().c_str(),
          info.find_data().dwFileAttributes & ~DWORD{FILE_ATTRIBUTE_READONLY});
    }

    DWORD this_result = ERROR_SUCCESS;
    if (info.IsDirectory()) {
      if (recursive) {
        this_result = DeleteFileRecursive(current, pattern, true);
        DCHECK_NE(static_cast<LONG>(this_result), ERROR_FILE_NOT_FOUND);
        DCHECK_NE(static_cast<LONG>(this_result), ERROR_PATH_NOT_FOUND);
        if (this_result == ERROR_SUCCESS &&
            !::RemoveDirectory(current.value().c_str())) {
          this_result = ReturnLastErrorOrSuccessOnNotFound();
        }
      }
    } else if (!::DeleteFile(current.value().c_str())) {
      this_result = ReturnLastErrorOrSuccessOnNotFound();
    }
    if (result == ERROR_SUCCESS)
      result = this_result;
  }
  return result;
}

// Returns ERROR_SUCCESS on success, or a Windows error code on failure.
DWORD DoDeleteFile(const FilePath& path, bool recursive) {
  if (path.empty())
    return ERROR_SUCCESS;

  if (path.value().length() >= MAX_PATH)
    return ERROR_BAD_PATHNAME;

  // Handle any path with wildcards.
  if (path.BaseName().value().find_first_of(FILE_PATH_LITERAL("*?")) !=
      FilePath::StringType::npos) {
    const DWORD error_code =
        DeleteFileRecursive(path.DirName(), path.BaseName().value(), recursive);
    DCHECK_NE(static_cast<LONG>(error_code), ERROR_FILE_NOT_FOUND);
    DCHECK_NE(static_cast<LONG>(error_code), ERROR_PATH_NOT_FOUND);
    return error_code;
  }

  // Report success if the file or path does not exist.
  const DWORD attr = ::GetFileAttributes(path.value().c_str());
  if (attr == INVALID_FILE_ATTRIBUTES)
    return ReturnLastErrorOrSuccessOnNotFound();

  // Clear the read-only bit if it is set.
  if ((attr & FILE_ATTRIBUTE_READONLY) &&
      !::SetFileAttributes(path.value().c_str(),
                           attr & ~DWORD{FILE_ATTRIBUTE_READONLY})) {
    // It's possible for |path| to be gone now under a race with other deleters.
    return ReturnLastErrorOrSuccessOnNotFound();
  }

  // Perform a simple delete on anything that isn't a directory.
  if (!(attr & FILE_ATTRIBUTE_DIRECTORY)) {
    return ::DeleteFile(path.value().c_str())
               ? ERROR_SUCCESS
               : ReturnLastErrorOrSuccessOnNotFound();
  }

  if (recursive) {
    const DWORD error_code =
        DeleteFileRecursive(path, FILE_PATH_LITERAL("*"), true);
    DCHECK_NE(static_cast<LONG>(error_code), ERROR_FILE_NOT_FOUND);
    DCHECK_NE(static_cast<LONG>(error_code), ERROR_PATH_NOT_FOUND);
    if (error_code != ERROR_SUCCESS)
      return error_code;
  }
  return ::RemoveDirectory(path.value().c_str())
             ? ERROR_SUCCESS
             : ReturnLastErrorOrSuccessOnNotFound();
}

// Deletes the file/directory at |path| (recursively if |recursive| and |path|
// names a directory), returning true on success. Sets the Windows last-error
// code and returns false on failure.
bool DeleteFileOrSetLastError(const FilePath& path, bool recursive) {
  const DWORD error = DoDeleteFile(path, recursive);
  if (error == ERROR_SUCCESS)
    return true;

  ::SetLastError(error);
  return false;
}

void AppendModeCharacter(wchar_t mode_char, std::wstring* mode) {
  size_t comma_pos = mode->find(L',');
  mode->insert(comma_pos == std::wstring::npos ? mode->length() : comma_pos, 1,
               mode_char);
}

bool DoCopyFile(const FilePath& from_path,
                const FilePath& to_path,
                bool fail_if_exists) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE,
  // BlockingType::MAY_BLOCK);
  if (from_path.ReferencesParent() || to_path.ReferencesParent())
    return false;

  // NOTE: I suspect we could support longer paths, but that would involve
  // analyzing all our usage of files.
  if (from_path.value().length() >= MAX_PATH ||
      to_path.value().length() >= MAX_PATH) {
    return false;
  }

  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  // SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();

  // Unlike the posix implementation that copies the file manually and discards
  // the ACL bits, CopyFile() copies the complete SECURITY_DESCRIPTOR and access
  // bits, which is usually not what we want. We can't do much about the
  // SECURITY_DESCRIPTOR but at least remove the read only bit.
  const wchar_t* dest = to_path.value().c_str();
  if (!::CopyFile(from_path.value().c_str(), dest, fail_if_exists)) {
    // Copy failed.
    return false;
  }
  DWORD attrs = GetFileAttributes(dest);
  if (attrs == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if (attrs & FILE_ATTRIBUTE_READONLY) {
    SetFileAttributes(dest, attrs & ~DWORD{FILE_ATTRIBUTE_READONLY});
  }
  return true;
}

}  // namespace

bool DeletePathRecursively(const FilePath& path) {
  return DeleteFileOrSetLastError(path, /*recursive=*/true);
}

bool CreateDirectoryAndGetError(const FilePath& full_path, File::Error* error) {
  // If the path exists, we've succeeded if it's a directory, failed otherwise.
  const wchar_t* const full_path_str = full_path.value().c_str();
  const DWORD fileattr = ::GetFileAttributes(full_path_str);
  if (fileattr != INVALID_FILE_ATTRIBUTES) {
    if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      return true;
    }
    if (error)
      *error = File::FILE_ERROR_NOT_A_DIRECTORY;
    ::SetLastError(ERROR_FILE_EXISTS);
    return false;
  }

  // Invariant:  Path does not exist as file or directory.

  // Attempt to create the parent recursively.  This will immediately return
  // true if it already exists, otherwise will create all required parent
  // directories starting with the highest-level missing parent.
  FilePath parent_path(full_path.DirName());
  if (parent_path.value() == full_path.value()) {
    if (error)
      *error = File::FILE_ERROR_NOT_FOUND;
    ::SetLastError(ERROR_FILE_NOT_FOUND);
    return false;
  }
  if (!CreateDirectoryAndGetError(parent_path, error)) {
    DLOG(WARNING) << "Failed to create one of the parent directories.";
    DCHECK(!error || *error != File::FILE_OK);
    return false;
  }

  if (::CreateDirectory(full_path_str, NULL))
    return true;

  const DWORD error_code = ::GetLastError();
  if (error_code == ERROR_ALREADY_EXISTS && DirectoryExists(full_path)) {
    // This error code ERROR_ALREADY_EXISTS doesn't indicate whether we were
    // racing with someone creating the same directory, or a file with the same
    // path.  If DirectoryExists() returns true, we lost the race to create the
    // same directory.
    return true;
  }
  if (error)
    *error = File::OSErrorToFileError(error_code);
  ::SetLastError(error_code);
  return false;
}

bool PathExists(const FilePath& path) {
  return (GetFileAttributes(path.value().c_str()) != INVALID_FILE_ATTRIBUTES);
}

bool DirectoryExists(const FilePath& path) {
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr != INVALID_FILE_ATTRIBUTES)
    return (fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0;
  return false;
}

#undef CopyFile
bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  return DoCopyFile(from_path, to_path, false);
}

namespace internal {
std::wstring UTF8ToWide(const std::string_view& utf8);
}

bool GetFileInfo(const FilePath& file_path, File::Info* results) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE,
  // BlockingType::MAY_BLOCK);

  WIN32_FILE_ATTRIBUTE_DATA attr;
  if (!GetFileAttributesEx(file_path.value().c_str(), GetFileExInfoStandard,
                           &attr)) {
    return false;
  }

  ULARGE_INTEGER size;
  size.HighPart = attr.nFileSizeHigh;
  size.LowPart = attr.nFileSizeLow;
  // TODO(crbug.com/40227936): Change Info::size to uint64_t and eliminate this
  // cast.
  results->size = checked_cast<int64_t>(size.QuadPart);

  results->is_directory =
      (attr.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
  // Time class is not supported yet.
  // results->last_modified = Time::FromFileTime(attr.ftLastWriteTime);
  // results->last_accessed = Time::FromFileTime(attr.ftLastAccessTime);
  // results->creation_time = Time::FromFileTime(attr.ftCreationTime);

  return true;
}

FILE* OpenFile(const FilePath& filename, const char* mode) {
  // 'N' is unconditionally added below, so be sure there is not one already
  // present before a comma in |mode|.
  DCHECK(
      strchr(mode, 'N') == nullptr ||
      (strchr(mode, ',') != nullptr && strchr(mode, 'N') > strchr(mode, ',')));
  // Do not check blocking call because it is not supported.
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE,
  // BlockingType::MAY_BLOCK);
  std::wstring w_mode = internal::UTF8ToWide(mode);
  AppendModeCharacter(L'N', &w_mode);
  return _wfsopen(filename.value().c_str(), w_mode.c_str(), _SH_DENYNO);
}

}  // namespace kiwi::base