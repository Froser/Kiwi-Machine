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

#ifndef BASE_FILES_FILE_H_
#define BASE_FILES_FILE_H_

#include <stdint.h>
#include <fstream>
#include <memory>

#include "base/base_export.h"
#include "base/platform/platform_factory.h"

namespace kiwi::base {
class FilePath;
class BASE_EXPORT File {
 public:
  enum Flags {
    FLAG_OPEN = 1 << 0,            // Opens a file, only if it exists.
    FLAG_CREATE = 1 << 1,          // Creates a new file, only if it does not
                                   // already exist.
    FLAG_OPEN_ALWAYS = 1 << 2,     // May create a new file.
    FLAG_CREATE_ALWAYS = 1 << 3,   // May overwrite an old file.
    FLAG_OPEN_TRUNCATED = 1 << 4,  // Opens a file and truncates it, only if it
                                   // exists.
    FLAG_READ = 1 << 5,
    FLAG_WRITE = 1 << 6,
    FLAG_APPEND = 1 << 7,
  };

  File(const FilePath& path, uint32_t flags);

 public:
  int ReadAtCurrentPos(char* data, int size);
  bool IsValid() const;

 private:
  std::unique_ptr<platform::FileInterface> file_;
};
}  // namespace kiwi::base

#endif  // BASE_FILES_FILE_H_
