// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_UTF_STRING_CONVERSIONS_H_
#define BASE_STRINGS_UTF_STRING_CONVERSIONS_H_

#include <stddef.h>

#include <string>
#include <string_view>

#include "base/base_export.h"
#include "base/strings/string_piece.h"

namespace kiwi::base {

// These convert between UTF-8, -16, and -32 strings. They are potentially slow,
// so avoid unnecessary conversions. The low-level versions return a boolean
// indicating whether the conversion was 100% valid. In this case, it will still
// do the best it can and put the result in the output buffer. The versions that
// return strings ignore this error and just return the best conversion
// possible.

[[nodiscard]] BASE_EXPORT std::string WideToUTF8(std::wstring_view wide);
[[nodiscard]] BASE_EXPORT std::wstring UTF8ToWide(std::string_view utf8);

#if defined(WCHAR_T_IS_16_BIT)
// Converts to 7-bit ASCII by truncating. The result must be known to be ASCII
// beforehand.
[[nodiscard]] BASE_EXPORT std::string WideToASCII(std::wstring_view wide);

// This converts an ASCII string, typically a hardcoded constant, to a wide
// string.
[[nodiscard]] BASE_EXPORT std::wstring ASCIIToWide(StringPiece ascii);
#endif  // defined(WCHAR_T_IS_16_BIT)

}  // namespace kiwi::base

#endif  // BASE_STRINGS_UTF_STRING_CONVERSIONS_H_
