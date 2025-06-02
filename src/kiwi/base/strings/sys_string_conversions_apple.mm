// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides system-dependent string type conversions for cases where it's
// necessary to not use ICU. Generally, you should not need this in Chrome,
// but it is used in some shared code. Dependencies should be minimal.

#include "base/strings/sys_string_conversions.h"

#import <Foundation/Foundation.h>
#include <stddef.h>

#include "base/apple/bridging.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/numerics/safe_conversions.h"

namespace kiwi::base {
namespace {

// Converts the supplied CFString into the specified encoding, and returns it as
// a C++ library string of the template type. Returns an empty string on
// failure.
//
// Do not assert in this function since it is used by the assertion code!
template <typename StringType>
StringType CFStringToStringWithEncodingT(CFStringRef cfstring,
                                         CFStringEncoding encoding) {
  CFIndex length = CFStringGetLength(cfstring);
  if (length == 0) {
    return StringType();
  }

  CFRange whole_string = CFRangeMake(0, length);
  CFIndex out_size;
  CFIndex converted = CFStringGetBytes(cfstring, whole_string, encoding,
                                       /*lossByte=*/0,
                                       /*isExternalRepresentation=*/false,
                                       /*buffer=*/nullptr,
                                       /*maxBufLen=*/0, &out_size);
  if (converted == 0 || out_size <= 0) {
    return StringType();
  }

  // `out_size` is the number of UInt8-sized units needed in the destination.
  // A buffer allocated as UInt8 units might not be properly aligned to
  // contain elements of StringType::value_type.  Use a container for the
  // proper value_type, and convert `out_size` by figuring the number of
  // value_type elements per UInt8.  Leave room for a NUL terminator.
  size_t elements = static_cast<size_t>(out_size) * sizeof(UInt8) /
                        sizeof(typename StringType::value_type) +
                    1;

  std::vector<typename StringType::value_type> out_buffer(elements);
  converted =
      CFStringGetBytes(cfstring, whole_string, encoding,
                       /*lossByte=*/0,
                       /*isExternalRepresentation=*/false,
                       reinterpret_cast<UInt8*>(&out_buffer[0]), out_size,
                       /*usedBufLen=*/nullptr);
  if (converted == 0) {
    return StringType();
  }

  out_buffer[elements - 1] = '\0';
  return StringType(&out_buffer[0], elements - 1);
}

std::string SysCFStringRefToUTF8(CFStringRef ref) {
  return CFStringToStringWithEncodingT<std::string>(ref, kCFStringEncodingUTF8);
}

// Given a StringPiece `in` with an encoding specified by `in_encoding`, returns
// it as a CFStringRef. Returns null on failure.
template <typename CharT>
apple::ScopedCFTypeRef<CFStringRef> StringPieceToCFStringWithEncodingsT(
    BasicStringPiece<CharT> in,
    CFStringEncoding in_encoding) {
  const auto in_length = in.length();
  if (in_length == 0) {
    return apple::ScopedCFTypeRef<CFStringRef>(CFSTR(""),
                                               base::scoped_policy::RETAIN);
  }

  return apple::ScopedCFTypeRef<CFStringRef>(CFStringCreateWithBytes(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(in.data()),
      checked_cast<CFIndex>(in_length * sizeof(CharT)), in_encoding, false));
}

}  // namespace

apple::ScopedCFTypeRef<CFStringRef> SysUTF8ToCFStringRef(StringPiece utf8) {
  return StringPieceToCFStringWithEncodingsT(utf8, kCFStringEncodingUTF8);
}

NSString* SysUTF8ToNSString(StringPiece utf8) {
  return base::apple::CFToNSOwnershipCast(SysUTF8ToCFStringRef(utf8).release());
}

std::string SysNSStringToUTF8(NSString* nsstring) {
  if (!nsstring) {
    return std::string();
  }
  return SysCFStringRefToUTF8(kiwi::base::apple::NSToCFPtrCast(nsstring));
}

}  // namespace kiwi::base
