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

#ifndef BASE_STRINGS_STRING_PIECE_H_
#define BASE_STRINGS_STRING_PIECE_H_

#include <string_view>

namespace kiwi::base {
template <typename CharT>
using BasicStringPiece = std::basic_string_view<CharT>;
using StringPiece = std::string_view;
using StringPiece16 = std::u16string_view;
using WStringPiece = std::wstring_view;
}  // namespace kiwi::base

#endif  // BASE_STRINGS_STRING_PIECE_H_