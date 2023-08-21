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

#include "base/platform/default/default_file_interface.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"

namespace kiwi::base {
namespace platform {
DefaultFileInterface::DefaultFileInterface() = default;
DefaultFileInterface::~DefaultFileInterface() {
  file_.close();
}

void DefaultFileInterface::Open(const FilePath& file_path, uint32_t flags) {
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
  file_.open(file_path, open_mode);
}

int DefaultFileInterface::ReadAtCurrentPos(char* data, int size) {
  DCHECK(IsValid());
  file_.read(data, size);
  return static_cast<int>(file_.gcount());
}

bool DefaultFileInterface::IsValid() const {
  return file_.is_open();
}

}  // namespace platform
}  // namespace kiwi::base