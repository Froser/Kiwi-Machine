// Copyright (C) 2026 Yisi Yu
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

#include <cstdio>
#include <sys/types.h>
#include <unistd.h>

extern "C" ssize_t pread(int fd,
                         void* buffer,
                         size_t count,
                         off_t offset) {
  const off_t original_offset = lseek(fd, 0, SEEK_CUR);
  if (original_offset == static_cast<off_t>(-1) ||
      lseek(fd, offset, SEEK_SET) == static_cast<off_t>(-1)) {
    return -1;
  }

  const ssize_t result = read(fd, buffer, count);
  if (lseek(fd, original_offset, SEEK_SET) == static_cast<off_t>(-1)) {
    return -1;
  }
  return result;
}

extern "C" ssize_t pwrite(int fd,
                          const void* buffer,
                          size_t count,
                          off_t offset) {
  const off_t original_offset = lseek(fd, 0, SEEK_CUR);
  if (original_offset == static_cast<off_t>(-1) ||
      lseek(fd, offset, SEEK_SET) == static_cast<off_t>(-1)) {
    return -1;
  }

  const ssize_t result = write(fd, buffer, count);
  if (lseek(fd, original_offset, SEEK_SET) == static_cast<off_t>(-1)) {
    return -1;
  }
  return result;
}
