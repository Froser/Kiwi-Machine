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

#include "base/base_export.h"
#include "base/strings/string_piece.h"

namespace kiwi::base {

BASE_EXPORT std::string NumberToString(int value);

BASE_EXPORT bool StringToUint64(StringPiece input, uint64_t* output);

BASE_EXPORT bool HexStringToUInt64(StringPiece input, uint64_t* output);

}  // namespace kiwi::base

#endif  // BASE_STRINGS_NUMBER_STRING_CONVERSIONS_