// Copyright (C) 2026 Yisi Yu
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

#ifndef NES_ROM_HASH_H_
#define NES_ROM_HASH_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include "nes/nes_export.h"

namespace kiwi {
namespace nes {

inline constexpr size_t kSha1DigestSize = 20;

// Calculates the 20-byte SHA-1 digest of |data|.
NES_EXPORT std::array<uint8_t, kSha1DigestSize> CalculateSha1(
    std::span<const uint8_t> data);

// Calculates the SHA-1 digest of |data| as 40 uppercase hexadecimal digits.
NES_EXPORT std::string CalculateSha1Hex(std::span<const uint8_t> data);

// Validates a 40-digit hexadecimal SHA-1 string and converts it to uppercase.
// Returns std::nullopt if |sha1| is not a valid SHA-1 representation.
NES_EXPORT std::optional<std::string> NormalizeSha1Hex(std::string sha1);

// Calculates the SHA-1 of the declared PRG-ROM followed by CHR-ROM, excluding
// the iNES header and trailing data. Returns std::nullopt for an invalid or
// unsupported iNES image.
NES_EXPORT std::optional<std::string> CalculateINESRomSha1Hex(
    std::span<const uint8_t> image);

// Returns the ROM SHA-1 used to locate an automatic .sav file. Returns
// std::nullopt if the iNES image is invalid, unsupported, or does not declare
// battery-backed memory.
NES_EXPORT std::optional<std::string> CalculateINESBatterySaveSha1Hex(
    std::span<const uint8_t> image);

}  // namespace nes
}  // namespace kiwi

#endif  // NES_ROM_HASH_H_
