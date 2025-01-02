// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#ifndef BASE_STRINGS_STRING_UTIL_H_
#define BASE_STRINGS_STRING_UTIL_H_

#include <ctype.h>
#include <stdarg.h>  // va_list
#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/cxx20_to_address.h"
#include "base/strings/string_piece.h"  // For implicit conversions.
#include "base/strings/string_util_internal.h"
#include "build/build_config.h"

namespace kiwi::base {

// C standard-library functions that aren't cross-platform are provided as
// "base::...", and their prototypes are listed below. These functions are
// then implemented as inline calls to the platform-specific equivalents in the
// platform-specific headers.

// Some of these implementations need to be inlined.

// We separate the declaration from the implementation of this inline
// function just so the PRINTF_FORMAT works.
inline int snprintf(char* buffer, size_t size, const char* format, ...)
    PRINTF_FORMAT(3, 4);
inline int snprintf(char* buffer, size_t size, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vsnprintf(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

// BSD-style safe and consistent string copy functions.
// Copies |src| to |dst|, where |dst_size| is the total allocated size of |dst|.
// Copies at most |dst_size|-1 characters, and always NULL terminates |dst|, as
// long as |dst_size| is not 0.  Returns the length of |src| in characters.
// If the return value is >= dst_size, then the output was truncated.
// NOTE: All sizes are in number of characters, NOT in bytes.
BASE_EXPORT size_t strlcpy(char* dst, const char* src, size_t dst_size);
BASE_EXPORT size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size);

// Scan a wprintf format string to determine whether it's portable across a
// variety of systems.  This function only checks that the conversion
// specifiers used by the format string are supported and have the same meaning
// on a variety of systems.  It doesn't check for other errors that might occur
// within a format string.
//
// Nonportable conversion specifiers for wprintf are:
//  - 's' and 'c' without an 'l' length modifier.  %s and %c operate on char
//     data on all systems except Windows, which treat them as wchar_t data.
//     Use %ls and %lc for wchar_t data instead.
//  - 'S' and 'C', which operate on wchar_t data on all systems except Windows,
//     which treat them as char data.  Use %ls and %lc for wchar_t data
//     instead.
//  - 'F', which is not identified by Windows wprintf documentation.
//  - 'D', 'O', and 'U', which are deprecated and not available on all systems.
//     Use %ld, %lo, and %lu instead.
//
// Note that there is no portable conversion specifier for char data when
// working with wprintf.
//
// This function is intended to be called from base::vswprintf.
BASE_EXPORT bool IsWprintfFormatPortable(const wchar_t* format);

// Simplified implementation of C++20's std::basic_string_view(It, End).
// Reference: https://wg21.link/string.view.cons
template <typename CharT, typename Iter>
BasicStringPiece<CharT> MakeBasicStringPiece(Iter begin, Iter end) {
  DCHECK_GE(end - begin, 0);
  return {base::to_address(begin), static_cast<size_t>(end - begin)};
}

// Explicit instantiations of MakeBasicStringPiece for the BasicStringPiece
// aliases defined in base/strings/string_piece_forward.h
template <typename Iter>
constexpr StringPiece MakeStringPiece(Iter begin, Iter end) {
  return MakeBasicStringPiece<char>(begin, end);
}

template <typename Iter>
constexpr StringPiece16 MakeStringPiece16(Iter begin, Iter end) {
  return MakeBasicStringPiece<char16_t>(begin, end);
}

template <typename Iter>
constexpr WStringPiece MakeWStringPiece(Iter begin, Iter end) {
  return MakeBasicStringPiece<wchar_t>(begin, end);
}

// Convert a type with defined `operator<<` into a string.
template <typename... Streamable>
std::string StreamableToString(const Streamable&... values) {
  std::ostringstream ss;
  (ss << ... << values);
  return ss.str();
}

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
template <typename CharT,
          typename = std::enable_if_t<std::is_integral<CharT>::value>>
constexpr CharT ToLowerASCII(CharT c) {
  return internal::ToLowerASCII(c);
}

// ASCII-specific toupper.  The standard library's toupper is locale sensitive,
// so we don't want to use it here.
template <typename CharT,
          typename = std::enable_if_t<std::is_integral<CharT>::value>>
CharT ToUpperASCII(CharT c) {
  return (c >= 'a' && c <= 'z') ? static_cast<CharT>(c + 'A' - 'a') : c;
}

// Converts the given string to it's ASCII-lowercase equivalent.
BASE_EXPORT std::string ToLowerASCII(StringPiece str);
BASE_EXPORT std::u16string ToLowerASCII(StringPiece16 str);

// Converts the given string to it's ASCII-uppercase equivalent.
BASE_EXPORT std::string ToUpperASCII(StringPiece str);
BASE_EXPORT std::u16string ToUpperASCII(StringPiece16 str);

// Functor for case-insensitive ASCII comparisons for STL algorithms like
// std::search.
//
// Note that a full Unicode version of this functor is not possible to write
// because case mappings might change the number of characters, depend on
// context (combining accents), and require handling UTF-16. If you need
// proper Unicode support, use base::i18n::ToLower/FoldCase and then just
// use a normal operator== on the result.
template <typename Char>
struct CaseInsensitiveCompareASCII {
 public:
  bool operator()(Char x, Char y) const {
    return ToLowerASCII(x) == ToLowerASCII(y);
  }
};

// Like strcasecmp for case-insensitive ASCII characters only. Returns:
//   -1  (a < b)
//    0  (a == b)
//    1  (a > b)
// (unlike strcasecmp which can return values greater or less than 1/-1). For
// full Unicode support, use base::i18n::ToLower or base::i18n::FoldCase
// and then just call the normal string operators on the result.
BASE_EXPORT constexpr int CompareCaseInsensitiveASCII(StringPiece a,
                                                      StringPiece b) {
  return internal::CompareCaseInsensitiveASCIIT(a, b);
}
BASE_EXPORT constexpr int CompareCaseInsensitiveASCII(StringPiece16 a,
                                                      StringPiece16 b) {
  return internal::CompareCaseInsensitiveASCIIT(a, b);
}

// Equality for ASCII case-insensitive comparisons. For full Unicode support,
// use base::i18n::ToLower or base::i18n::FoldCase and then compare with either
// == or !=.
inline bool EqualsCaseInsensitiveASCII(StringPiece a, StringPiece b) {
  return internal::EqualsCaseInsensitiveASCIIT(a, b);
}
inline bool EqualsCaseInsensitiveASCII(StringPiece16 a, StringPiece16 b) {
  return internal::EqualsCaseInsensitiveASCIIT(a, b);
}

// Contains the set of characters representing whitespace in the corresponding
// encoding. Null-terminated. The ASCII versions are the whitespaces as defined
// by HTML5, and don't include control characters.
BASE_EXPORT extern const wchar_t kWhitespaceWide[];    // Includes Unicode.
BASE_EXPORT extern const char16_t kWhitespaceUTF16[];  // Includes Unicode.
BASE_EXPORT extern const char16_t
    kWhitespaceNoCrLfUTF16[];  // Unicode w/o CR/LF.
BASE_EXPORT extern const char kWhitespaceASCII[];
BASE_EXPORT extern const char16_t kWhitespaceASCIIAs16[];  // No unicode.

// Null-terminated string representing the UTF-8 byte order mark.
BASE_EXPORT extern const char kUtf8ByteOrderMark[];

enum TrimPositions {
  TRIM_NONE = 0,
  TRIM_LEADING = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL = TRIM_LEADING | TRIM_TRAILING,
};

// Removes characters in |trim_chars| from the beginning and end of |input|.
// The 8-bit version only works on 8-bit characters, not UTF-8. Returns true if
// any characters were removed.
//
// It is safe to use the same variable for both |input| and |output| (this is
// the normal usage to trim in-place).
BASE_EXPORT bool TrimString(StringPiece16 input,
                            StringPiece16 trim_chars,
                            std::u16string* output);
BASE_EXPORT bool TrimString(StringPiece input,
                            StringPiece trim_chars,
                            std::string* output);

// StringPiece versions of the above. The returned pieces refer to the original
// buffer.
BASE_EXPORT StringPiece16 TrimString(StringPiece16 input,
                                     StringPiece16 trim_chars,
                                     TrimPositions positions);
BASE_EXPORT StringPiece TrimString(StringPiece input,
                                   StringPiece trim_chars,
                                   TrimPositions positions);

// Truncates a string to the nearest UTF-8 character that will leave
// the string less than or equal to the specified byte size.
BASE_EXPORT void TruncateUTF8ToByteSize(const std::string& input,
                                        const size_t byte_size,
                                        std::string* output);

// Trims any whitespace from either end of the input string.
//
// The StringPiece versions return a substring referencing the input buffer.
// The ASCII versions look only for ASCII whitespace.
//
// The std::string versions return where whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
BASE_EXPORT TrimPositions TrimWhitespace(StringPiece16 input,
                                         TrimPositions positions,
                                         std::u16string* output);
BASE_EXPORT StringPiece16 TrimWhitespace(StringPiece16 input,
                                         TrimPositions positions);
BASE_EXPORT TrimPositions TrimWhitespaceASCII(StringPiece input,
                                              TrimPositions positions,
                                              std::string* output);
BASE_EXPORT StringPiece TrimWhitespaceASCII(StringPiece input,
                                            TrimPositions positions);

enum class CompareCase {
  SENSITIVE,
  INSENSITIVE_ASCII,
};

BASE_EXPORT bool StartsWith(
    StringPiece str,
    StringPiece search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);
BASE_EXPORT bool StartsWith(
    StringPiece16 str,
    StringPiece16 search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);
BASE_EXPORT bool EndsWith(
    StringPiece str,
    StringPiece search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);
BASE_EXPORT bool EndsWith(
    StringPiece16 str,
    StringPiece16 search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);

// Determines the type of ASCII character, independent of locale (the C
// library versions will change based on locale).
template <typename Char>
inline bool IsAsciiWhitespace(Char c) {
  // kWhitespaceASCII is a null-terminated string.
  for (const char* cur = kWhitespaceASCII; *cur; ++cur) {
    if (*cur == c)
      return true;
  }
  return false;
}
template <typename Char>
inline bool IsAsciiAlpha(Char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
template <typename Char>
inline bool IsAsciiUpper(Char c) {
  return c >= 'A' && c <= 'Z';
}
template <typename Char>
inline bool IsAsciiLower(Char c) {
  return c >= 'a' && c <= 'z';
}
template <typename Char>
inline bool IsAsciiDigit(Char c) {
  return c >= '0' && c <= '9';
}
template <typename Char>
inline bool IsAsciiAlphaNumeric(Char c) {
  return IsAsciiAlpha(c) || IsAsciiDigit(c);
}
template <typename Char>
inline bool IsAsciiPrintable(Char c) {
  return c >= ' ' && c <= '~';
}

template <typename Char>
inline bool IsHexDigit(Char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

// Returns the integer corresponding to the given hex character. For example:
//    '4' -> 4
//    'a' -> 10
//    'B' -> 11
// Assumes the input is a valid hex character.
BASE_EXPORT char HexDigitToInt(char c);
inline char HexDigitToInt(char16_t c) {
  // DCHECK(IsHexDigit(c));
  return HexDigitToInt(static_cast<char>(c));
}

// Returns whether `c` is a Unicode whitespace character.
// This cannot be used on eight-bit characters, since if they are ASCII you
// should call IsAsciiWhitespace(), and if they are from a UTF-8 string they may
// be individual units of a multi-unit code point.  Convert to 16- or 32-bit
// values known to hold the full code point before calling this.
template <typename Char, typename = std::enable_if_t<(sizeof(Char) > 1)>>
inline bool IsUnicodeWhitespace(Char c) {
  // kWhitespaceWide is a null-terminated string.
  for (const auto* cur = kWhitespaceWide; *cur; ++cur) {
    if (static_cast<typename std::make_unsigned_t<wchar_t>>(*cur) ==
        static_cast<typename std::make_unsigned_t<Char>>(c))
      return true;
  }
  return false;
}

// DANGEROUS: Assumes ASCII or not base on the size of `Char`.  You should
// probably be explicitly calling IsUnicodeWhitespace() or IsAsciiWhitespace()
// instead!
template <typename Char>
inline bool IsWhitespace(Char c) {
  if constexpr (sizeof(Char) > 1) {
    return IsUnicodeWhitespace(c);
  } else {
    return IsAsciiWhitespace(c);
  }
}

}  // namespace kiwi::base

#endif  // BASE_STRINGS_STRING_UTIL_H_
