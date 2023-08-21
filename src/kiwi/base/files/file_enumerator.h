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

#ifndef BASE_FILES_FILE_ENUMERATOR_H_
#define BASE_FILES_FILE_ENUMERATOR_H_

#include <filesystem>

#include "base/base_export.h"
#include "base/files/file_path.h"

namespace kiwi::base {
class BASE_EXPORT FileEnumerator {
 public:
  enum FileType {
    FILES = 1 << 0,
    DIRECTORIES = 1 << 1,
  };

  class BASE_EXPORT FileInfo {
   public:
    FileInfo();
    ~FileInfo();

    bool IsDirectory() const;

    int64_t GetSize() const;

   private:
    friend class FileEnumerator;
    const FileEnumerator* fe_ = nullptr;
  };

  friend class FileInfo;

  // |file_type| is OR combination of FileEnumerator::FileType.
  FileEnumerator(const FilePath& root_path, bool recursive, int file_type);

  // Returns the next file or an empty string if there are no more results.
  //
  // The returned path will incorporate the |root_path| passed in the
  // constructor: "<root_path>/file_name.txt". If the |root_path| is absolute,
  // then so will be the result of Next().
  FilePath Next();

  // Returns info about the file last returned by Next()
  FileInfo GetInfo() const;

 private:
  bool recursive_ = false;
  int file_type_ = FILES;
  std::filesystem::directory_iterator di_, diend_;
  std::filesystem::recursive_directory_iterator rdi_, rdi_end_;
  std::unique_ptr<std::filesystem::directory_entry> find_data_;
};
}  // namespace kiwi::base

#endif  // BASE_FILES_FILE_ENUMERATOR_H_
