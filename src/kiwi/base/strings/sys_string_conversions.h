// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_SYS_STRING_CONVERSIONS_H_
#define BASE_STRINGS_SYS_STRING_CONVERSIONS_H_

// Provides system-dependent string type conversions for cases where it's
// necessary to not use ICU. Generally, you should not need this in Chrome,
// but it is used in some shared code. Dependencies should be minimal.

#include <stdint.h>

#include <string>

#include "base/base_export.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <CoreFoundation/CoreFoundation.h>

#include "base/apple/scoped_cftyperef.h"

#ifdef __OBJC__
@class NSString;
#endif
#endif  // BUILDFLAG(IS_APPLE)

namespace kiwi::base {

// Windows-specific ------------------------------------------------------------

#if BUILDFLAG(IS_WIN)

// Converts between 8-bit and wide strings, using the given code page. The
// code page identifier is one accepted by the Windows function
// MultiByteToWideChar().
[[nodiscard]] BASE_EXPORT std::wstring SysMultiByteToWide(StringPiece mb,
                                                          uint32_t code_page);
[[nodiscard]] BASE_EXPORT std::string SysWideToMultiByte(
    const std::wstring& wide,
    uint32_t code_page);

#endif  // BUILDFLAG(IS_WIN)

// Mac-specific ----------------------------------------------------------------

#if BUILDFLAG(IS_APPLE)

// Converts between strings and CFStringRefs/NSStrings.
// Converts a string to a CFStringRef. Returns null on failure.
[[nodiscard]] BASE_EXPORT apple::ScopedCFTypeRef<CFStringRef>
SysUTF8ToCFStringRef(StringPiece utf8);

#ifdef __OBJC__

// Converts a string to an autoreleased NSString. Returns nil on failure.
[[nodiscard]] BASE_EXPORT NSString* SysUTF8ToNSString(StringPiece utf8);

// Converts an NSString to a string. Returns an empty string on failure or if
// `ref` is nil.
[[nodiscard]] BASE_EXPORT std::string SysNSStringToUTF8(NSString* ref);

#endif  // __OBJC__

#endif  // BUILDFLAG(IS_APPLE)

}  // namespace kiwi::base

#endif  // BASE_STRINGS_SYS_STRING_CONVERSIONS_H_
