// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"

#include <copyfile.h>

#include "base/files/file_path.h"

namespace kiwi::base {

bool CopyFile(const FilePath& from_path, const FilePath& to_path) {
  // ScopedBlockingCall scoped_blocking_call(FROM_HERE,
  // BlockingType::MAY_BLOCK);
  if (from_path.ReferencesParent() || to_path.ReferencesParent()) {
    return false;
  }
  return (copyfile(from_path.value().c_str(), to_path.value().c_str(),
                   /*state=*/nullptr, COPYFILE_DATA) == 0);
}

}  // namespace kiwi::base
