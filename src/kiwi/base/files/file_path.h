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

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/strings/string_piece.h"

// Windows-style drive letter support and pathname separator characters can be
// enabled and disabled independently, to aid testing.  These #defines are
// here so that the same setting can be used in both the implementation and
// in the unit test.
#if BUILDFLAG(IS_WIN)
#define FILE_PATH_USES_DRIVE_LETTERS
#define FILE_PATH_USES_WIN_SEPARATORS
#endif  // BUILDFLAG(IS_WIN)

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

class BASE_EXPORT FilePath {
 public:
#if BUILDFLAG(IS_WIN)
  // On Windows, for Unicode-aware applications, native pathnames are wchar_t
  // arrays encoded in UTF-16.
  typedef std::wstring StringType;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // On most platforms, native pathnames are char arrays, and the encoding
  // may or may not be specified.  On Mac OS X, native pathnames are encoded
  // in UTF-8.
  typedef std::string StringType;
#endif  // BUILDFLAG(IS_WIN)

  typedef StringType::value_type CharType;
  typedef BasicStringPiece<CharType> StringPieceType;

  // Null-terminated array of separators used to separate components in paths.
  // Each character in this array is a valid separator, but kSeparators[0] is
  // treated as the canonical separator and is used when composing pathnames.
  static constexpr CharType kSeparators[] =
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
      FILE_PATH_LITERAL("\\/");
#else   // FILE_PATH_USES_WIN_SEPARATORS
      FILE_PATH_LITERAL("/");
#endif  // FILE_PATH_USES_WIN_SEPARATORS

  // std::size(kSeparators), i.e., the number of separators in kSeparators plus
  // one (the null terminator at the end of kSeparators).
  static constexpr size_t kSeparatorsLength = std::size(kSeparators);

  // The special path component meaning "this directory."
  static constexpr CharType kCurrentDirectory[] = FILE_PATH_LITERAL(".");

  // The special path component meaning "the parent directory."
  static constexpr CharType kParentDirectory[] = FILE_PATH_LITERAL("..");

  // The character used to identify a file extension.
  static constexpr CharType kExtensionSeparator = FILE_PATH_LITERAL('.');

  FilePath();
  FilePath(const FilePath& that);
  explicit FilePath(StringPieceType path);
  ~FilePath();
  FilePath& operator=(const FilePath& that);

  // Constructs FilePath with the contents of |that|, which is left in valid but
  // unspecified state.
  FilePath(FilePath&& that) noexcept;
  // Replaces the contents with those of |that|, which is left in valid but
  // unspecified state.
  FilePath& operator=(FilePath&& that) noexcept;

  bool operator==(const FilePath& that) const;

  bool operator!=(const FilePath& that) const;

  // Required for some STL containers and operations
  bool operator<(const FilePath& that) const { return path_ < that.path_; }

  const StringType& value() const { return path_; }

  [[nodiscard]] bool empty() const { return path_.empty(); }

  void clear() { path_.clear(); }

  // Returns true if |character| is in kSeparators.
  static bool IsSeparator(CharType character);

  // Returns the extension of a file path.  This method works very similarly to
  // FinalExtension(), except when the file path ends with a common
  // double-extension.  For common double-extensions like ".tar.gz" and
  // ".user.js", this method returns the combined extension.
  //
  // Common means that detecting double-extensions is based on a hard-coded
  // allow-list (including but not limited to ".*.gz" and ".user.js") and isn't
  // solely dependent on the number of dots.  Specifically, even if somebody
  // invents a new Blah compression algorithm:
  //   - calling this function with "foo.tar.bz2" will return ".tar.bz2", but
  //   - calling this function with "foo.tar.blah" will return just ".blah"
  //     until ".*.blah" is added to the hard-coded allow-list.
  //
  // That hard-coded allow-list is case-insensitive: ".GZ" and ".gz" are
  // equivalent. However, the StringType returned is not canonicalized for
  // case: "foo.TAR.bz2" input will produce ".TAR.bz2", not ".tar.bz2", and
  // "bar.EXT", which is not a double-extension, will produce ".EXT".
  //
  // The following code should always work regardless of the value of path.
  //   new_path = path.RemoveExtension().value().append(path.Extension());
  //   ASSERT(new_path == path.value());
  //
  // NOTE: this is different from the original file_util implementation which
  // returned the extension without a leading "." ("jpg" instead of ".jpg").
  [[nodiscard]] StringType Extension() const;

  [[nodiscard]] FilePath Append(StringPieceType component) const;

 private:
  void StripTrailingSeparatorsInternal();

 public:
  // Returns a copy of this FilePath that does not end with a trailing
  // separator.
  [[nodiscard]] FilePath StripTrailingSeparators() const;

  std::string AsUTF8Unsafe() const;

  static FilePath FromUTF8Unsafe(StringPiece utf8);

  [[nodiscard]] FilePath Append(const FilePath& component) const;

  // Returns true if this FilePath contains an attempt to reference a parent
  // directory (e.g. has a path component that is "..").
  bool ReferencesParent() const;

  // Returns a vector of all of the components of the provided path. It is
  // equivalent to calling DirName().value() on the path's root component,
  // and BaseName().value() on each child component.
  //
  // To make sure this is lossless so we can differentiate absolute and
  // relative paths, the root slash will be included even though no other
  // slashes will be. The precise behavior is:
  //
  // Posix:  "/foo/bar"  ->  [ "/", "foo", "bar" ]
  // Windows:  "C:\foo\bar"  ->  [ "C:", "\\", "foo", "bar" ]
  std::vector<FilePath::StringType> GetComponents() const;

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

 private:
  StringType path_;
};

BASE_EXPORT std::ostream& operator<<(std::ostream& out,
                                     const FilePath& file_path);

}  // namespace kiwi::base

#endif  // BASE_FILES_FILE_PATH_H_
