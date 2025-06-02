// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_UTIL_WIN_H_
#define BASE_STRINGS_STRING_UTIL_WIN_H_

#include <string>

#include "base/base_export.h"
#include "base/strings/string_util.h"

namespace kiwi::base {

// Utility functions to convert between base::WStringPiece and
// base::StringPiece16.
inline const char16_t* as_u16cstr(const wchar_t* str) {
  return reinterpret_cast<const char16_t*>(str);
}

inline StringPiece16 AsStringPiece16(WStringPiece str) {
  return StringPiece16(as_u16cstr(str.data()), str.size());
}

BASE_EXPORT std::wstring ToLowerASCII(std::wstring_view str);

BASE_EXPORT WStringPiece TrimWhitespace(WStringPiece input,
                                        TrimPositions positions);

BASE_EXPORT bool StartsWith(
    std::wstring_view str,
    std::wstring_view search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);

inline bool EqualsCaseInsensitiveASCII(std::wstring_view a,
                                       std::wstring_view b) {
  return internal::EqualsCaseInsensitiveASCIIT(a, b);
}

BASE_EXPORT wchar_t* WriteInto(std::wstring* str, size_t length_with_null);

BASE_EXPORT void ReplaceFirstSubstringAfterOffset(
    std::wstring* str,
    size_t start_offset,
    std::wstring_view find_this,
    std::wstring_view replace_with);

}  // namespace kiwi::base

#endif  // !BASE_STRINGS_STRING_UTIL_WIN_H_