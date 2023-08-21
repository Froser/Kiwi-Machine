// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#include <limits>
#include <type_traits>
#include <vector>

#include "base/strings/string_util_impl_helpers.h"
#include "base/strings/string_util_internal.h"
#include "build/build_config.h"

namespace kiwi::base {

bool IsWprintfFormatPortable(const wchar_t* format) {
  for (const wchar_t* position = format; *position != '\0'; ++position) {
    if (*position == '%') {
      bool in_specification = true;
      bool modifier_l = false;
      while (in_specification) {
        // Eat up characters until reaching a known specifier.
        if (*++position == '\0') {
          // The format string ended in the middle of a specification.  Call
          // it portable because no unportable specifications were found.  The
          // string is equally broken on all platforms.
          return true;
        }

        if (*position == 'l') {
          // 'l' is the only thing that can save the 's' and 'c' specifiers.
          modifier_l = true;
        } else if (((*position == 's' || *position == 'c') && !modifier_l) ||
                   *position == 'S' || *position == 'C' || *position == 'F' ||
                   *position == 'D' || *position == 'O' || *position == 'U') {
          // Not portable.
          return false;
        }

        if (wcschr(L"diouxXeEfgGaAcspn%", *position)) {
          // Portable, keep scanning the rest of the format string.
          in_specification = false;
        }
      }
    }
  }

  return true;
}

bool TrimString(StringPiece16 input,
                StringPiece16 trim_chars,
                std::u16string* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

bool TrimString(StringPiece input,
                StringPiece trim_chars,
                std::string* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

std::string ToLowerASCII(StringPiece str) {
  return internal::ToLowerASCIIImpl(str);
}

std::u16string ToLowerASCII(StringPiece16 str) {
  return internal::ToLowerASCIIImpl(str);
}

std::string ToUpperASCII(StringPiece str) {
  return internal::ToUpperASCIIImpl(str);
}

std::u16string ToUpperASCII(StringPiece16 str) {
  return internal::ToUpperASCIIImpl(str);
}

StringPiece16 TrimString(StringPiece16 input,
                         StringPiece16 trim_chars,
                         TrimPositions positions) {
  return internal::TrimStringPieceT(input, trim_chars, positions);
}

StringPiece TrimString(StringPiece input,
                       StringPiece trim_chars,
                       TrimPositions positions) {
  return internal::TrimStringPieceT(input, trim_chars, positions);
}

TrimPositions TrimWhitespace(StringPiece16 input,
                             TrimPositions positions,
                             std::u16string* output) {
  return internal::TrimStringT(input, StringPiece16(kWhitespaceUTF16),
                               positions, output);
}

StringPiece16 TrimWhitespace(StringPiece16 input, TrimPositions positions) {
  return internal::TrimStringPieceT(input, StringPiece16(kWhitespaceUTF16),
                                    positions);
}

TrimPositions TrimWhitespaceASCII(StringPiece input,
                                  TrimPositions positions,
                                  std::string* output) {
  return internal::TrimStringT(input, StringPiece(kWhitespaceASCII), positions,
                               output);
}

StringPiece TrimWhitespaceASCII(StringPiece input, TrimPositions positions) {
  return internal::TrimStringPieceT(input, StringPiece(kWhitespaceASCII),
                                    positions);
}

std::u16string CollapseWhitespace(StringPiece16 text,
                                  bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

std::string CollapseWhitespaceASCII(StringPiece text,
                                    bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

bool ContainsOnlyChars(StringPiece input, StringPiece characters) {
  return input.find_first_not_of(characters) == StringPiece::npos;
}

bool ContainsOnlyChars(StringPiece16 input, StringPiece16 characters) {
  return input.find_first_not_of(characters) == StringPiece16::npos;
}

bool StartsWith(StringPiece str,
                StringPiece search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool StartsWith(StringPiece16 str,
                StringPiece16 search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(StringPiece str,
              StringPiece search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(StringPiece16 str,
              StringPiece16 search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

char HexDigitToInt(char c) {
  DCHECK(IsHexDigit(c));
  if (c >= '0' && c <= '9')
    return static_cast<char>(c - '0');
  return (c >= 'A' && c <= 'F') ? static_cast<char>(c - 'A' + 10)
                                : static_cast<char>(c - 'a' + 10);
}

size_t strlcpy(char* dst, const char* src, size_t dst_size) {
  return internal::lcpyT(dst, src, dst_size);
}
size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size) {
  return internal::lcpyT(dst, src, dst_size);
}

}  // namespace kiwi::base
