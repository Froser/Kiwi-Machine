// Copyright (C) 2025 Yisi Yu
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

#include "base/strings/string_util_win.h"

#include "base/strings/string_util_impl_helpers.h"

namespace kiwi::base {

std::wstring ToLowerASCII(std::wstring_view str) {
  return internal::ToLowerASCIIImpl(str);
}

WStringPiece TrimWhitespace(WStringPiece input, TrimPositions positions) {
  return internal::TrimStringPieceT(input, WStringPiece(kWhitespaceWide),
                                    positions);
}

std::wstring CollapseWhitespace(WStringPiece text,
                                bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

bool StartsWith(std::wstring_view str,
                std::wstring_view search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

wchar_t* WriteInto(std::wstring* str, size_t length_with_null) {
  return internal::WriteIntoT(str, length_with_null);
}

void ReplaceFirstSubstringAfterOffset(std::wstring* str,
                                      size_t start_offset,
                                      std::wstring_view find_this,
                                      std::wstring_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

}  // namespace kiwi::base