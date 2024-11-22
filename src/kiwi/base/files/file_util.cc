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

#include "base/files/file_util.h"

#include <span>
#include <functional>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"

namespace kiwi::base {

namespace {
bool ReadStreamToSpanWithMaxSize(
    FILE* stream,
    size_t max_size,
    std::function<std::span<uint8_t>(size_t)> resize_span) {
  if (!stream) {
    return false;
  }

  // Seeking to the beginning is best-effort -- it is expected to fail for
  // certain non-file stream (e.g., pipes).
  HANDLE_EINTR(fseek(stream, 0, SEEK_SET));

  // Many files have incorrect size (proc files etc). Hence, the file is read
  // sequentially as opposed to a one-shot read, using file size as a hint for
  // chunk size if available.
  constexpr size_t kDefaultChunkSize = 1 << 16;
  size_t chunk_size = kDefaultChunkSize - 1;
// Do not check blocking call because it is not supported.
// ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
#if BUILDFLAG(IS_WIN)
  BY_HANDLE_FILE_INFORMATION file_info = {};
  if (::GetFileInformationByHandle(
          reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stream))),
          &file_info)) {
    LARGE_INTEGER size;
    size.HighPart = static_cast<LONG>(file_info.nFileSizeHigh);
    size.LowPart = file_info.nFileSizeLow;
    if (size.QuadPart > 0) {
      chunk_size = static_cast<size_t>(size.QuadPart);
    }
  }
#else   // BUILDFLAG(IS_WIN)
  // In cases where the reported file size is 0, use a smaller chunk size to
  // minimize memory allocated and cost of string::resize() in case the read
  // size is small (i.e. proc files). If the file is larger than this, the read
  // loop will reset |chunk_size| to kDefaultChunkSize.
  constexpr size_t kSmallChunkSize = 4096;
  chunk_size = kSmallChunkSize - 1;
  stat_wrapper_t file_info = {};
  if (!File::Fstat(fileno(stream), &file_info) && file_info.st_size > 0) {
    chunk_size = static_cast<size_t>(file_info.st_size);
  }
#endif  // BUILDFLAG(IS_WIN)

  // We need to attempt to read at EOF for feof flag to be set so here we use
  // |chunk_size| + 1.
  chunk_size = std::min(chunk_size, max_size) + 1;
  size_t bytes_read_this_pass;
  size_t bytes_read_so_far = 0;
  bool read_status = true;
  std::span<uint8_t> bytes_span = resize_span(chunk_size);
  DCHECK_EQ(bytes_span.size(), chunk_size);

  while ((bytes_read_this_pass = fread(bytes_span.data() + bytes_read_so_far, 1,
                                       chunk_size, stream)) > 0) {
    if ((max_size - bytes_read_so_far) < bytes_read_this_pass) {
      // Read more than max_size bytes, bail out.
      bytes_read_so_far = max_size;
      read_status = false;
      break;
    }
    // In case EOF was not reached, iterate again but revert to the default
    // chunk size.
    if (bytes_read_so_far == 0) {
      chunk_size = kDefaultChunkSize;
    }

    bytes_read_so_far += bytes_read_this_pass;
    // Last fread syscall (after EOF) can be avoided via feof, which is just a
    // flag check.
    if (feof(stream)) {
      break;
    }
    bytes_span = resize_span(bytes_read_so_far + chunk_size);
    DCHECK_EQ(bytes_span.size(), bytes_read_so_far + chunk_size);
  }
  read_status = read_status && !ferror(stream);

  // Trim the container down to the number of bytes that were actually read.
  bytes_span = resize_span(bytes_read_so_far);
  DCHECK_EQ(bytes_span.size(), bytes_read_so_far);

  return read_status;
}

}  // namespace

bool CreateDirectory(const FilePath& full_path) {
  return CreateDirectoryAndGetError(full_path, nullptr);
}

std::optional<std::vector<uint8_t>> ReadFileToBytes(const FilePath& path) {
  if (path.ReferencesParent()) {
    return std::nullopt;
  }

  ScopedFILE file_stream(OpenFile(path, "rb"));
  if (!file_stream) {
    return std::nullopt;
  }

  std::vector<uint8_t> bytes;
  if (!ReadStreamToSpanWithMaxSize(file_stream.get(),
                                   std::numeric_limits<size_t>::max(),
                                   [&bytes](size_t size) {
                                     bytes.resize(size);
                                     return std::span(bytes);
                                   })) {
    return std::nullopt;
  }
  return bytes;
}

bool CloseFile(FILE* file) {
  if (file == nullptr) {
    return true;
  }
  return fclose(file) == 0;
}

}  // namespace kiwi::base