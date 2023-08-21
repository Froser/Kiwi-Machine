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

#include "base/platform/qt/qt_file_interface.h"
#include "base/files/file.h"
#include "base/files/file_path.h"

namespace kiwi::base {
namespace platform {
QtFileInterface::QtFileInterface() = default;
QtFileInterface::~QtFileInterface() {
  file_.close();
}

void QtFileInterface::Open(const FilePath& file_path, uint32_t flags) {
  // |flags| refers to base::File::Flags.
  // Transform these flags to QFile::Flags.

  file_.setFileName(QString::fromUtf8(file_path.u8string()));
  QIODevice::OpenMode open_mode;
  open_mode |= ((flags & File::Flags::FLAG_OPEN) ? QIODevice::ReadOnly
                                                 : QIODevice::NotOpen);
  open_mode |= ((flags & File::Flags::FLAG_CREATE) ? QIODevice::NewOnly
                                                   : QIODevice::NotOpen);
  open_mode |= ((flags & File::Flags::FLAG_OPEN_ALWAYS) ? QIODevice::ReadOnly
                                                        : QIODevice::NotOpen);
  open_mode |= ((flags & File::Flags::FLAG_CREATE_ALWAYS) ? QIODevice::WriteOnly
                                                          : QIODevice::NotOpen);
  open_mode |=
      ((flags & File::Flags::FLAG_OPEN_TRUNCATED) ? QIODevice::Truncate
                                                  : QIODevice::NotOpen);
  open_mode |= ((flags & File::Flags::FLAG_READ) ? QIODevice::ReadOnly
                                                 : QIODevice::NotOpen);
  open_mode |= ((flags & File::Flags::FLAG_WRITE) ? QIODevice::WriteOnly
                                                  : QIODevice::NotOpen);
  open_mode |= ((flags & File::Flags::FLAG_APPEND) ? QIODevice::Append
                                                   : QIODevice::NotOpen);
  file_.open(open_mode);
}

int QtFileInterface::ReadAtCurrentPos(char* data, int size) {
  return file_.read(data, size);
}

bool QtFileInterface::IsValid() const {
  return file_.isOpen();
}
}  // namespace platform
}  // namespace kiwi::base
