// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file.h"

#include <windows.h>

#include "base/check.h"

namespace kiwi::base {
// Static.
File::Error File::OSErrorToFileError(DWORD last_error) {
  switch (last_error) {
    case ERROR_SHARING_VIOLATION:
    case ERROR_UNABLE_TO_REMOVE_REPLACED:  // ReplaceFile failure cases.
    case ERROR_UNABLE_TO_MOVE_REPLACEMENT:
    case ERROR_UNABLE_TO_MOVE_REPLACEMENT_2:
      return FILE_ERROR_IN_USE;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
      return FILE_ERROR_EXISTS;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return FILE_ERROR_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
    case ERROR_LOCK_VIOLATION:
      return FILE_ERROR_ACCESS_DENIED;
    case ERROR_TOO_MANY_OPEN_FILES:
      return FILE_ERROR_TOO_MANY_OPENED;
    case ERROR_OUTOFMEMORY:
    case ERROR_NOT_ENOUGH_MEMORY:
      return FILE_ERROR_NO_MEMORY;
    case ERROR_HANDLE_DISK_FULL:
    case ERROR_DISK_FULL:
    case ERROR_DISK_RESOURCES_EXHAUSTED:
      return FILE_ERROR_NO_SPACE;
    case ERROR_USER_MAPPED_FILE:
      return FILE_ERROR_INVALID_OPERATION;
    case ERROR_NOT_READY:         // The device is not ready.
    case ERROR_SECTOR_NOT_FOUND:  // The drive cannot find the sector requested.
    case ERROR_GEN_FAILURE:       // A device ... is not functioning.
    case ERROR_DEV_NOT_EXIST:  // Net resource or device is no longer available.
    case ERROR_IO_DEVICE:
    case ERROR_DISK_OPERATION_FAILED:
    case ERROR_FILE_CORRUPT:  // File or directory is corrupted and unreadable.
    case ERROR_DISK_CORRUPT:  // The disk structure is corrupted and unreadable.
      return FILE_ERROR_IO;
    default:
      // This function should only be called for errors.
      DCHECK_NE(static_cast<DWORD>(ERROR_SUCCESS), last_error);
      return FILE_ERROR_FAILED;
  }
}

}  // namespace kiwi::base
