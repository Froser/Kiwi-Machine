// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file_enumerator.h"
#include "base/check.h"

namespace kiwi::base {
FileEnumerator::FileInfo::FileInfo() = default;
FileEnumerator::FileInfo::~FileInfo() = default;

bool FileEnumerator::FileInfo::IsDirectory() const {
  DCHECK(fe_ && fe_->find_data_);
  return fe_->find_data_->is_directory();
}

int64_t FileEnumerator::FileInfo::GetSize() const {
  DCHECK(fe_ && fe_->find_data_);
  return fe_->find_data_->file_size();
}

FileEnumerator::FileEnumerator(const FilePath& root_path,
                               bool recursive,
                               int file_type)
    : recursive_(recursive), file_type_(file_type) {
  if (!recursive) {
    di_ = std::filesystem::directory_iterator(root_path);
  } else {
    rdi_ = std::filesystem::recursive_directory_iterator(root_path);
  }
}

FilePath FileEnumerator::Next() {
  FilePath path;
  if (!recursive_) {
    if (di_ != std::filesystem::directory_iterator()) {
      std::filesystem::directory_entry entry = *di_;
      ++di_;
      if (((file_type_ & FILES) && entry.is_regular_file()) ||
          (file_type_ & DIRECTORIES) && entry.is_directory()) {
        path = entry.path();
        find_data_ = std::make_unique<std::filesystem::directory_entry>(entry);
      } else {
        return Next();
      }
    }
  } else {
    if (rdi_ != std::filesystem::recursive_directory_iterator()) {
      std::filesystem::directory_entry entry = *rdi_;
      ++rdi_;
      if (((file_type_ & FILES) && entry.is_regular_file()) ||
          (file_type_ & DIRECTORIES) && entry.is_directory()) {
        path = entry.path();
        find_data_ = std::make_unique<std::filesystem::directory_entry>(entry);
      } else {
        return Next();
      }
    }
  }
  return path;
}

FileEnumerator::FileInfo FileEnumerator::GetInfo() const {
  FileEnumerator::FileInfo fi;
  fi.fe_ = this;
  return fi;
}

}  // namespace kiwi::base
