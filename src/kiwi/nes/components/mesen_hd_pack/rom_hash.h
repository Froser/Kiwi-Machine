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

#ifndef NES_COMPONENTS_MESEN_HD_PACK_ROM_HASH_H_
#define NES_COMPONENTS_MESEN_HD_PACK_ROM_HASH_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace kiwi {
namespace nes {
namespace mesen_hd_pack {

inline constexpr size_t kSha1DigestSize = 20;

std::array<uint8_t, kSha1DigestSize> CalculateSha1(
    std::span<const uint8_t> data);
std::string CalculateSha1Hex(std::span<const uint8_t> data);

}  // namespace mesen_hd_pack
}  // namespace nes
}  // namespace kiwi

#endif  // NES_COMPONENTS_MESEN_HD_PACK_ROM_HASH_H_
