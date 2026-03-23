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

#ifndef UTILITY_ALGORITMN_H_
#define UTILITY_ALGORITMN_H_

#include <string>
#include <vector>

#include <kiwi_nes.h>

// Checks if string s1 contains all characters of string s2 in the same order.
// The comparison is case-insensitive.
// 
// `s1` is The source string to search in.
// `s2` is The string to search for.
// Return true if s1 contains all characters of s2 in order, false otherwise
bool HasString(const std::string& s1, const std::string& s2);

// Encodes binary data to Base64 string.
// `data` is the binary data to encode.
// `len` is the length of the data.
// Returns the Base64 encoded string.
std::string Base64Encode(const kiwi::nes::Byte* data, size_t len);

#endif  // UTILITY_ALGORITMN_H_