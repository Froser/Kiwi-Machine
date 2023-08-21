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

#ifndef BASE_FILES_FILE_PATH_H_
#define BASE_FILES_FILE_PATH_H_

#include <filesystem>

#include "base/base_export.h"
#include "base/strings/string_piece.h"

namespace kiwi::base {
class BASE_EXPORT FilePath : public std::filesystem::path {
  using std::filesystem::path::path;
  using StringType = std::filesystem::path::string_type;

 public:
  FilePath(const std::filesystem::path& path);

  std::string AsUTF8Unsafe() const;

  static FilePath FromUTF8Unsafe(StringPiece utf8);

  [[nodiscard]] FilePath Append(const FilePath& component) const;

  // Returns a FilePath corresponding to the last path component of this
  // object, either a file or a directory.  If this object already refers to
  // the root directory, returns a FilePath identifying the root directory;
  // this is the only situation in which BaseName will return an absolute path.
  [[nodiscard]] FilePath BaseName() const;

  // Returns the final extension of a file path, or an empty string if the file
  // path has no extension.  In most cases, the final extension of a file path
  // refers to the part of the file path from the last dot to the end (including
  // the dot itself).  For example, this method applied to "/pics/jojo.jpg"
  // and "/pics/jojo." returns ".jpg" and ".", respectively.  However, if the
  // base name of the file path is either "." or "..", this method returns an
  // empty string.
  [[nodiscard]] StringType FinalExtension() const;
};
}  // namespace kiwi::base

#endif  // BASE_FILES_FILE_PATH_H_
