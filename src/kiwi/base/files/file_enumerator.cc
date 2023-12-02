// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file_enumerator.h"
#include "base/check.h"

namespace kiwi::base {

FileEnumerator::FileInfo::~FileInfo() = default;

bool FileEnumerator::ShouldSkip(const FilePath& path) {
  FilePath::StringType basename = path.BaseName().value();
  return basename == FILE_PATH_LITERAL(".") ||
         (basename == FILE_PATH_LITERAL("..") &&
          !(INCLUDE_DOT_DOT & file_type_));
}

bool FileEnumerator::IsTypeMatched(bool is_dir) const {
  return (file_type_ &
          (is_dir ? FileEnumerator::DIRECTORIES : FileEnumerator::FILES)) != 0;
}

}  // namespace kiwi::base
