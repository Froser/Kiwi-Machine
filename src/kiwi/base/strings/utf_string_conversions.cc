// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"

#include <codecvt>

#include "base/check.h"
#include "build/build_config.h"

namespace kiwi::base {

std::string WideToUTF8(std::wstring_view wide) {
#if BUILDFLAG(IS_WIN)
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  return converter.to_bytes(wide.data());
#else
  CHECK(false) << "Not implement.";
#endif
}

std::wstring UTF8ToWide(std::string_view utf8) {
#if BUILDFLAG(IS_WIN)
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
  return converter.from_bytes(utf8.data());
#else
  CHECK(false) << "Not implement.";
#endif
}

#if defined(WCHAR_T_IS_16_BIT)
std::string WideToASCII(std::wstring_view wide) {
  // DCHECK(IsStringASCII(wide)) << wide;
  return std::string(wide.begin(), wide.end());
}

std::wstring ASCIIToWide(StringPiece ascii) {
  // DCHECK(IsStringASCII(ascii)) << ascii;
  return std::wstring(ascii.begin(), ascii.end());
}
#endif  // defined(WCHAR_T_IS_16_BIT)

}  // namespace kiwi::base
