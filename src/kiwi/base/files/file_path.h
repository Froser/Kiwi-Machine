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
#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"

namespace kiwi::base {

#if BUILDFLAG(IS_WIN)

// The `FILE_PATH_LITERAL_INTERNAL` indirection allows `FILE_PATH_LITERAL` to
// work correctly with macro parameters, for example
// `FILE_PATH_LITERAL(TEST_FILE)` where `TEST_FILE` is a macro #defined as
// "TestFile".
#define FILE_PATH_LITERAL_INTERNAL(x) L##x
#define FILE_PATH_LITERAL(x) FILE_PATH_LITERAL_INTERNAL(x)

#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#define FILE_PATH_LITERAL(x) x
#endif  // BUILDFLAG(IS_WIN)

class BASE_EXPORT FilePath : public std::filesystem::path {
  using std::filesystem::path::path;
  using StringType = std::filesystem::path::string_type;

 public:
  FilePath(const std::filesystem::path& path);

  std::string AsUTF8Unsafe() const;

  static FilePath FromUTF8Unsafe(StringPiece utf8);

  [[nodiscard]] FilePath Append(const FilePath& component) const;

  // Returns a FilePath corresponding to the directory containing the path
  // named by this object, stripping away the file component.  If this object
  // only contains one component, returns a FilePath identifying
  // kCurrentDirectory.  If this object already refers to the root directory,
  // returns a FilePath identifying the root directory. Please note that this
  // doesn't resolve directory navigation, e.g. the result for "../a" is "..".
  [[nodiscard]] FilePath DirName() const;

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

  // Returns "C:\pics\jojo" for path "C:\pics\jojo.jpg"
  [[nodiscard]] FilePath RemoveExtension() const;

};
}  // namespace kiwi::base

#endif  // BASE_FILES_FILE_PATH_H_
