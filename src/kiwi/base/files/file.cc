// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is used for debugging assertion support.  The Lock class
// is functionally a wrapper around the LockImpl class, so the only
// real intelligence in the class is in the debugging logic.

#include "base/files/file.h"
#include "base/check.h"
#include "base/files/file_path.h"

namespace kiwi::base {
File::Info::Info() = default;

File::Info::~Info() = default;

File::File(const FilePath& path, uint32_t flags) {
  Open(path, flags);
}

File::~File() {
  file_.close();
}

void File::Open(const FilePath& file_path, uint32_t flags) {
  // Only few flags is useful.
  std::ios_base::openmode open_mode = std::ios_base::binary;
  open_mode |= ((flags & File::Flags::FLAG_OPEN) ? std::ios_base::in
                                                 : std::ios_base::openmode());
  open_mode |= ((flags & File::Flags::FLAG_CREATE) ? std::ios_base::out
                                                   : std::ios_base::openmode());
  open_mode |=
      ((flags & File::Flags::FLAG_OPEN_ALWAYS) ? std::ios_base::in
                                               : std::ios_base::openmode());
  open_mode |=
      ((flags & File::Flags::FLAG_CREATE_ALWAYS) ? std::ios_base::out
                                                 : std::ios_base::openmode());
  open_mode |=
      ((flags & File::Flags::FLAG_OPEN_TRUNCATED) ? std::ios_base::trunc
                                                  : std::ios_base::openmode());
  open_mode |= ((flags & File::Flags::FLAG_READ) ? std::ios_base::in
                                                 : std::ios_base::openmode());
  open_mode |= ((flags & File::Flags::FLAG_WRITE) ? std::ios_base::out
                                                  : std::ios_base::openmode());
  open_mode |= ((flags & File::Flags::FLAG_APPEND) ? std::ios_base::app
                                                   : std::ios_base::openmode());
  file_.open(file_path.AsUTF8Unsafe(), open_mode);
}

int64_t File::GetLength() {
  auto tmp = file_.tellg();
  file_.seekg(0, std::ios::end);
  auto size = file_.tellg();
  file_.seekg(tmp, std::ios::beg);
  return size;
}

int File::Read(int64_t offset, char* data, int size) {
  auto tmp = file_.tellg();
  file_.seekg(offset, std::ios::beg);
  file_.read(data, size);
  size_t count = file_.gcount();
  file_.seekg(tmp, std::ios::beg);
  return count;
}

int File::ReadAtCurrentPos(char* data, int size) {
  DCHECK(IsValid());
  file_.read(data, size);
  return static_cast<int>(file_.gcount());
}

int File::Write(int64_t offset, const char* data, int size) {
  auto tmp = file_.tellp();
  file_.seekp(offset, std::ios::cur);
  file_.write(data, size);
  file_.seekp(tmp, std::ios::cur);
  return size;
}

int File::WriteAtCurrentPos(const char* data, int size) {
  file_.write(data, size);
  return size;
}

int64_t File::Seek(Whence whence, int64_t offset) {
  switch (whence) {
    case FROM_BEGIN:
      file_.seekg(offset, std::ios::beg);
      break;
    case FROM_CURRENT:
      file_.seekg(offset, std::ios::cur);
      break;
    case FROM_END:
      file_.seekg(offset, std::ios::end);
      break;
    default:
      DCHECK(false) << "Shouldn't happen.";
      return -1;
  }
  return file_.tellg();
}

bool File::IsValid() const {
  return file_.is_open();
}
}  // namespace kiwi::base
