// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file.h"
#include "base/platform/platform_factory.h"

namespace kiwi::base {
File::File(const FilePath& path, uint32_t flags) {
  file_ = GetPlatformFactory()->CreateFileInterface(path, flags);
}

int File::ReadAtCurrentPos(char* data, int size) {
  return file_->ReadAtCurrentPos(data, size);
}

bool File::IsValid() const {
  return file_->IsValid();
}
}  // namespace kiwi::base
