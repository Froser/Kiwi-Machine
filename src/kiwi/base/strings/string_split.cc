// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_split_internal.h"

namespace kiwi::base {

namespace {

bool AppendStringKeyValue(StringPiece input,
                          char delimiter,
                          StringPairs* result) {
  // Always append a new item regardless of success (it might be empty). The
  // below code will copy the strings directly into the result pair.
  result->resize(result->size() + 1);
  auto& result_pair = result->back();

  // Find the delimiter.
  size_t end_key_pos = input.find_first_of(delimiter);
  if (end_key_pos == std::string::npos) {
    DVLOG(1) << "cannot find delimiter in: " << input;
    return false;    // No delimiter.
  }
  result_pair.first = std::string(input.substr(0, end_key_pos));

  // Find the value string.
  StringPiece remains = input.substr(end_key_pos, input.size() - end_key_pos);
  size_t begin_value_pos = remains.find_first_not_of(delimiter);
  if (begin_value_pos == StringPiece::npos) {
    DVLOG(1) << "cannot parse value from input: " << input;
    return false;   // No value.
  }

  result_pair.second = std::string(
      remains.substr(begin_value_pos, remains.size() - begin_value_pos));

  return true;
}

}  // namespace

std::vector<std::string> SplitString(StringPiece input,
                                     StringPiece separators,
                                     WhitespaceHandling whitespace,
                                     SplitResult result_type) {
  return internal::SplitStringT<std::string>(input, separators, whitespace,
                                             result_type);
}

std::vector<std::u16string> SplitString(StringPiece16 input,
                                        StringPiece16 separators,
                                        WhitespaceHandling whitespace,
                                        SplitResult result_type) {
  return internal::SplitStringT<std::u16string>(input, separators, whitespace,
                                                result_type);
}

std::vector<StringPiece> SplitStringPiece(StringPiece input,
                                          StringPiece separators,
                                          WhitespaceHandling whitespace,
                                          SplitResult result_type) {
  return internal::SplitStringT<StringPiece>(input, separators, whitespace,
                                             result_type);
}

std::vector<StringPiece16> SplitStringPiece(StringPiece16 input,
                                            StringPiece16 separators,
                                            WhitespaceHandling whitespace,
                                            SplitResult result_type) {
  return internal::SplitStringT<StringPiece16>(input, separators, whitespace,
                                               result_type);
}

}  // namespace kiwi::base
