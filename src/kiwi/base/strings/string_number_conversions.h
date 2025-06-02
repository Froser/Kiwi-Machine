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

#ifndef BASE_STRINGS_NUMBER_STRING_CONVERSIONS_
#define BASE_STRINGS_NUMBER_STRING_CONVERSIONS_

#include <span>
#include <string>

#include "base/base_export.h"
#include "base/strings/string_piece.h"

namespace kiwi::base {

BASE_EXPORT std::string NumberToString(int value);
BASE_EXPORT std::string NumberToString(unsigned int value);
BASE_EXPORT std::string NumberToString(unsigned long value);
BASE_EXPORT std::string NumberToString(int64_t value);
BASE_EXPORT std::string NumberToString(uint64_t value);

BASE_EXPORT bool StringToUint(StringPiece input, unsigned* output);

BASE_EXPORT bool StringToUint64(StringPiece input, uint64_t* output);

BASE_EXPORT bool HexStringToUInt64(StringPiece input, uint64_t* output);

BASE_EXPORT bool HexStringToInt(StringPiece input, int32_t* output);
BASE_EXPORT bool HexStringToUInt(StringPiece input, uint32_t* output);

// For floating-point conversions, only conversions of input strings in decimal
// form are defined to work.  Behavior with strings representing floating-point
// numbers in hexadecimal, and strings representing non-finite values (such as
// NaN and inf) is undefined.  Otherwise, these behave the same as the integral
// variants.  This expects the input string to NOT be specific to the locale.
// If your input is locale specific, use ICU to read the number.
// WARNING: Will write to |output| even when returning false.
//          Read the comments here and above StringToInt() carefully.
BASE_EXPORT bool StringToDouble(StringPiece input, double* output);
// Hex encoding ----------------------------------------------------------------

// Returns a hex string representation of a binary buffer. The returned hex
// string will be in upper case. This function does not check if |size| is
// within reasonable limits since it's written with trusted data in mind.  If
// you suspect that the data you want to format might be large, the absolute
// max size for |size| should be is
//   std::numeric_limits<size_t>::max() / 2
BASE_EXPORT std::string HexEncode(const void* bytes, size_t size);
BASE_EXPORT std::string HexEncode(std::span<const uint8_t> bytes);

}  // namespace kiwi::base

#endif  // BASE_STRINGS_NUMBER_STRING_CONVERSIONS_