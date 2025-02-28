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

#include "base/files/file_path.h"

#include <algorithm>

#include "base/check.h"
#include "base/strings/string_util.h"

#if BUILDFLAG(IS_WIN)
#include <codecvt>
#endif

namespace kiwi::base {

using StringType = FilePath::StringType;
using StringPieceType = FilePath::StringPieceType;

namespace internal {

#if BUILDFLAG(IS_WIN)

std::string WideToUTF8(const std::wstring_view& wide) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  return converter.to_bytes(wide.data());
}

std::wstring UTF8ToWide(const std::string_view& utf8) {
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  return converter.from_bytes(utf8.data());
}

#endif

}

namespace {
// If this FilePath contains a drive letter specification, returns the
// position of the last character of the drive letter specification,
// otherwise returns npos.  This can only be true on Windows, when a pathname
// begins with a letter followed by a colon.  On other platforms, this always
// returns npos.
StringPieceType::size_type FindDriveLetter(StringPieceType path) {
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  // This is dependent on an ASCII-based character set, but that's a
  // reasonable assumption.  iswalpha can be too inclusive here.
  if (path.length() >= 2 && path[1] == L':' &&
      ((path[0] >= L'A' && path[0] <= L'Z') ||
       (path[0] >= L'a' && path[0] <= L'z'))) {
    return 1;
  }
#endif  // FILE_PATH_USES_DRIVE_LETTERS
  return StringType::npos;
}

bool IsPathAbsolute(StringPieceType path) {
#if defined(FILE_PATH_USES_DRIVE_LETTERS)
  StringType::size_type letter = FindDriveLetter(path);
  if (letter != StringType::npos) {
    // Look for a separator right after the drive specification.
    return path.length() > letter + 1 &&
           FilePath::IsSeparator(path[letter + 1]);
  }
  // Look for a pair of leading separators.
  return path.length() > 1 && FilePath::IsSeparator(path[0]) &&
         FilePath::IsSeparator(path[1]);
#else   // FILE_PATH_USES_DRIVE_LETTERS
  // Look for a separator in the first position.
  return path.length() > 0 && FilePath::IsSeparator(path[0]);
#endif  // FILE_PATH_USES_DRIVE_LETTERS
}

bool AreAllSeparators(const StringType& input) {
  for (auto it : input) {
    if (!FilePath::IsSeparator(it))
      return false;
  }

  return true;
}

FilePath::StringType::size_type FinalExtensionSeparatorPosition(
    const FilePath::StringType& path) {
  // Special case "." and ".."
  if (path == FilePath::kCurrentDirectory || path == FilePath::kParentDirectory)
    return FilePath::StringType::npos;

  return path.rfind(FilePath::kExtensionSeparator);
}

// Same as above, but allow a second extension component of up to 4
// characters when the rightmost extension component is a common double
// extension (gz, bz2, Z).  For example, foo.tar.gz or foo.tar.Z would have
// extension components of '.tar.gz' and '.tar.Z' respectively.
FilePath::StringType::size_type ExtensionSeparatorPosition(
    const FilePath::StringType& path) {
  const FilePath::StringType::size_type last_dot =
      FinalExtensionSeparatorPosition(path);

  // No extension, or the extension is the whole filename.
  if (last_dot == FilePath::StringType::npos || last_dot == 0U)
    return last_dot;

  const FilePath::StringType::size_type penultimate_dot =
      path.rfind(FilePath::kExtensionSeparator, last_dot - 1);
  const FilePath::StringType::size_type last_separator = path.find_last_of(
      FilePath::kSeparators, last_dot - 1, FilePath::kSeparatorsLength - 1);

  if (penultimate_dot == FilePath::StringType::npos ||
      (last_separator != FilePath::StringType::npos &&
       penultimate_dot < last_separator)) {
    return last_dot;
  }

  return last_dot;
}
}  // namespace

const FilePath::CharType kStringTerminator = FILE_PATH_LITERAL('\0');

FilePath::FilePath() = default;
FilePath::~FilePath() = default;

FilePath::FilePath(const FilePath& that) = default;
FilePath::FilePath(FilePath&& that) noexcept = default;

FilePath::FilePath(StringPieceType path) : path_(path) {
  StringType::size_type nul_pos = path_.find(kStringTerminator);
  if (nul_pos != StringType::npos)
    path_.erase(nul_pos, StringType::npos);
}

FilePath& FilePath::operator=(const FilePath& that) = default;

FilePath& FilePath::operator=(FilePath&& that) noexcept = default;

bool FilePath::operator==(const FilePath& that) const {
  return path_ == that.path_;
}

bool FilePath::operator!=(const FilePath& that) const {
  return path_ != that.path_;
}

// Returns a copy of this FilePath that does not end with a trailing
// separator.
FilePath FilePath::StripTrailingSeparators() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  return new_path;
}

std::string FilePath::AsUTF8Unsafe() const {
#if BUILDFLAG(IS_WIN)
  return internal::WideToUTF8(value());
#else
  return value();
#endif
}

FilePath FilePath::DirName() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  // The drive letter, if any, always needs to remain in the output.  If there
  // is no drive letter, as will always be the case on platforms which do not
  // support drive letters, letter will be npos, or -1, so the comparisons and
  // resizes below using letter will still be valid.
  StringType::size_type letter = FindDriveLetter(new_path.path_);

  StringType::size_type last_separator = new_path.path_.find_last_of(
      kSeparators, StringType::npos, kSeparatorsLength - 1);
  if (last_separator == StringType::npos) {
    // path_ is in the current directory.
    new_path.path_.resize(letter + 1);
  } else if (last_separator == letter + 1) {
    // path_ is in the root directory.
    new_path.path_.resize(letter + 2);
  } else if (last_separator == letter + 2 &&
             IsSeparator(new_path.path_[letter + 1])) {
    // path_ is in "//" (possibly with a drive letter); leave the double
    // separator intact indicating alternate root.
    new_path.path_.resize(letter + 3);
  } else if (last_separator != 0) {
    // path_ is somewhere else, trim the basename.
    new_path.path_.resize(last_separator);
  }

  new_path.StripTrailingSeparatorsInternal();
  if (!new_path.path_.length())
    new_path.path_ = kCurrentDirectory;

  return new_path;
}

FilePath FilePath::BaseName() const {
  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  // The drive letter, if any, is always stripped.
  StringType::size_type letter = FindDriveLetter(new_path.path_);
  if (letter != StringType::npos) {
    new_path.path_.erase(0, letter + 1);
  }

  // Keep everything after the final separator, but if the pathname is only
  // one character and it's a separator, leave it alone.
  StringType::size_type last_separator = new_path.path_.find_last_of(
      kSeparators, StringType::npos, kSeparatorsLength - 1);
  if (last_separator != StringType::npos &&
      last_separator < new_path.path_.length() - 1) {
    new_path.path_.erase(0, last_separator + 1);
  }

  return new_path;
}

std::ostream& operator<<(std::ostream& out, const FilePath& file_path) {
  return out << file_path.AsUTF8Unsafe();
}

// static
bool FilePath::IsSeparator(FilePath::CharType character) {
  for (size_t i = 0; i < kSeparatorsLength - 1; ++i) {
    if (character == kSeparators[i]) {
      return true;
    }
  }

  return false;
}

StringType FilePath::Extension() const {
  FilePath base(BaseName());
  const StringType::size_type dot = ExtensionSeparatorPosition(base.path_);
  if (dot == StringType::npos)
    return StringType();

  return base.path_.substr(dot, StringType::npos);
}

FilePath::StringType FilePath::FinalExtension() const {
  FilePath base(BaseName());
  const StringType::size_type dot = FinalExtensionSeparatorPosition(base.path_);
  if (dot == StringType::npos)
    return StringType();

  return base.path_.substr(dot, StringType::npos);
}

FilePath FilePath::RemoveExtension() const {
  if (Extension().empty())
    return *this;

  const StringType::size_type dot = ExtensionSeparatorPosition(path_);
  if (dot == StringType::npos)
    return *this;

  return FilePath(path_.substr(0, dot));
}

FilePath FilePath::FromUTF8Unsafe(StringPiece utf8) {
#if BUILDFLAG(IS_WIN)
  return FilePath(internal::UTF8ToWide(utf8));
#else
  return FilePath(utf8);
#endif
}

[[nodiscard]] FilePath FilePath::Append(StringPieceType component) const {
  StringPieceType appended = component;
  StringType without_nuls;

  StringType::size_type nul_pos = component.find(kStringTerminator);
  if (nul_pos != StringPieceType::npos) {
    without_nuls = StringType(component.substr(0, nul_pos));
    appended = StringPieceType(without_nuls);
  }

  DCHECK(!IsPathAbsolute(appended));

  if (path_.compare(kCurrentDirectory) == 0 && !appended.empty()) {
    // Append normally doesn't do any normalization, but as a special case,
    // when appending to kCurrentDirectory, just return a new path for the
    // component argument.  Appending component to kCurrentDirectory would
    // serve no purpose other than needlessly lengthening the path, and
    // it's likely in practice to wind up with FilePath objects containing
    // only kCurrentDirectory when calling DirName on a single relative path
    // component.
    return FilePath(appended);
  }

  FilePath new_path(path_);
  new_path.StripTrailingSeparatorsInternal();

  // Don't append a separator if the path is empty (indicating the current
  // directory) or if the path component is empty (indicating nothing to
  // append).
  if (!appended.empty() && !new_path.path_.empty()) {
    // Don't append a separator if the path still ends with a trailing
    // separator after stripping (indicating the root directory).
    if (!IsSeparator(new_path.path_.back())) {
      // Don't append a separator if the path is just a drive letter.
      if (FindDriveLetter(new_path.path_) + 1 != new_path.path_.length()) {
        new_path.path_.append(1, kSeparators[0]);
      }
    }
  }

  new_path.path_.append(appended.data(), appended.size());
  return new_path;
}

FilePath FilePath::Append(const FilePath& component) const {
  return Append(component.value());
}

bool FilePath::ReferencesParent() const {
  if (path_.find(kParentDirectory) == StringType::npos) {
    // GetComponents is quite expensive, so avoid calling it in the majority
    // of cases where there isn't a kParentDirectory anywhere in the path.
    return false;
  }

  const std::vector<StringType> components = GetComponents();
  return std::any_of(
      components.begin(), components.end(), [](const StringType& component) {
#if BUILDFLAG(IS_WIN)
        // Windows has odd, undocumented behavior with path components
        // containing only whitespace and . characters. So, if all we see is .
        // and whitespace, then we treat any .. sequence as referencing parent.
        return component.find_first_not_of(FILE_PATH_LITERAL(". \n\r\t")) ==
                   std::string::npos &&
               component.find(kParentDirectory) != std::string::npos;
#else
        return component == kParentDirectory;
#endif
      });
}
std::vector<FilePath::StringType> FilePath::GetComponents() const {
  std::vector<StringType> ret_val;
  if (value().empty())
    return ret_val;

  FilePath current = *this;
  FilePath base;

  // Capture path components.
  while (current != current.DirName()) {
    base = current.BaseName();
    if (!AreAllSeparators(base.value()))
      ret_val.push_back(base.value());
    current = current.DirName();
  }

  // Capture root, if any.
  base = current.BaseName();
  if (!base.value().empty() && base.value() != kCurrentDirectory)
    ret_val.push_back(current.BaseName().value());

  // Capture drive letter, if any.
  FilePath dir = current.DirName();
  StringType::size_type letter = FindDriveLetter(dir.value());
  if (letter != StringType::npos)
    ret_val.emplace_back(dir.value(), 0, letter + 1);

  std::reverse(ret_val.begin(), ret_val.end());

  return ret_val;
}

void FilePath::StripTrailingSeparatorsInternal() {
  // If there is no drive letter, start will be 1, which will prevent stripping
  // the leading separator if there is only one separator.  If there is a drive
  // letter, start will be set appropriately to prevent stripping the first
  // separator following the drive letter, if a separator immediately follows
  // the drive letter.
  StringType::size_type start = FindDriveLetter(path_) + 2;

  StringType::size_type last_stripped = StringType::npos;
  for (StringType::size_type pos = path_.length();
       pos > start && IsSeparator(path_[pos - 1]); --pos) {
    // If the string only has two separators and they're at the beginning,
    // don't strip them, unless the string began with more than two separators.
    if (pos != start + 1 || last_stripped == start + 2 ||
        !IsSeparator(path_[start - 1])) {
      path_.resize(pos - 1);
      last_stripped = pos;
    }
  }
}

}  // namespace kiwi::base