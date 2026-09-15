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

#include "nes/rom_hash.h"

#include <algorithm>
#include <bit>
#include <cctype>
#include <cstring>
#include <vector>

namespace kiwi {
namespace nes {
namespace {

constexpr size_t kSha1BlockSize = 64;
constexpr size_t kINESHeaderSize = 16;
constexpr size_t kPRGROMBankSize = 16 * 1024;
constexpr size_t kCHRROMBankSize = 8 * 1024;

uint32_t ReadBigEndian32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

void ProcessBlock(const uint8_t* block, std::array<uint32_t, 5>* state) {
  std::array<uint32_t, 80> words = {};
  for (size_t i = 0; i < 16; ++i) {
    words[i] = ReadBigEndian32(block + i * 4);
  }
  for (size_t i = 16; i < words.size(); ++i) {
    words[i] = std::rotl(
        words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
  }

  uint32_t a = (*state)[0];
  uint32_t b = (*state)[1];
  uint32_t c = (*state)[2];
  uint32_t d = (*state)[3];
  uint32_t e = (*state)[4];

  for (size_t i = 0; i < words.size(); ++i) {
    uint32_t function = 0;
    uint32_t constant = 0;
    if (i < 20) {
      function = (b & c) | ((~b) & d);
      constant = 0x5a827999;
    } else if (i < 40) {
      function = b ^ c ^ d;
      constant = 0x6ed9eba1;
    } else if (i < 60) {
      function = (b & c) | (b & d) | (c & d);
      constant = 0x8f1bbcdc;
    } else {
      function = b ^ c ^ d;
      constant = 0xca62c1d6;
    }

    const uint32_t next = std::rotl(a, 5) + function + e + constant + words[i];
    e = d;
    d = c;
    c = std::rotl(b, 30);
    b = a;
    a = next;
  }

  (*state)[0] += a;
  (*state)[1] += b;
  (*state)[2] += c;
  (*state)[3] += d;
  (*state)[4] += e;
}

}  // namespace

std::array<uint8_t, kSha1DigestSize> CalculateSha1(
    std::span<const uint8_t> data) {
  std::array<uint32_t, 5> state = {
      0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0,
  };

  const size_t full_block_size = data.size() - data.size() % kSha1BlockSize;
  for (size_t offset = 0; offset < full_block_size; offset += kSha1BlockSize) {
    ProcessBlock(data.data() + offset, &state);
  }

  const size_t remaining_size = data.size() - full_block_size;
  const size_t tail_size =
      remaining_size < 56 ? kSha1BlockSize : kSha1BlockSize * 2;
  std::vector<uint8_t> tail(tail_size, 0);
  if (remaining_size > 0) {
    std::memcpy(tail.data(), data.data() + full_block_size, remaining_size);
  }
  tail[remaining_size] = 0x80;

  const uint64_t bit_length = static_cast<uint64_t>(data.size()) * 8;
  for (size_t i = 0; i < 8; ++i) {
    tail[tail_size - 1 - i] =
        static_cast<uint8_t>(bit_length >> static_cast<uint32_t>(i * 8));
  }
  for (size_t offset = 0; offset < tail.size(); offset += kSha1BlockSize) {
    ProcessBlock(tail.data() + offset, &state);
  }

  std::array<uint8_t, kSha1DigestSize> digest = {};
  for (size_t i = 0; i < state.size(); ++i) {
    digest[i * 4] = static_cast<uint8_t>(state[i] >> 24);
    digest[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
    digest[i * 4 + 2] = static_cast<uint8_t>(state[i] >> 8);
    digest[i * 4 + 3] = static_cast<uint8_t>(state[i]);
  }
  return digest;
}

std::string CalculateSha1Hex(std::span<const uint8_t> data) {
  static constexpr char kHexDigits[] = "0123456789ABCDEF";
  const std::array<uint8_t, kSha1DigestSize> digest = CalculateSha1(data);

  std::string result(kSha1DigestSize * 2, '0');
  for (size_t i = 0; i < digest.size(); ++i) {
    result[i * 2] = kHexDigits[digest[i] >> 4];
    result[i * 2 + 1] = kHexDigits[digest[i] & 0x0f];
  }
  return result;
}

std::optional<std::string> NormalizeSha1Hex(std::string sha1) {
  if (sha1.size() != kSha1DigestSize * 2 ||
      !std::all_of(sha1.begin(), sha1.end(),
                   [](unsigned char c) { return std::isxdigit(c) != 0; })) {
    return std::nullopt;
  }

  std::transform(sha1.begin(), sha1.end(), sha1.begin(), [](unsigned char c) {
    return static_cast<char>(std::toupper(c));
  });
  return sha1;
}

std::optional<std::string> CalculateINESRomSha1Hex(
    std::span<const uint8_t> image) {
  if (image.size() < kINESHeaderSize ||
      std::memcmp(image.data(), "NES\x1A", 4) != 0 || (image[6] & 0x04) != 0) {
    return std::nullopt;
  }

  const size_t content_size = static_cast<size_t>(image[4]) * kPRGROMBankSize +
                              static_cast<size_t>(image[5]) * kCHRROMBankSize;
  if (content_size == 0 || image.size() < kINESHeaderSize + content_size) {
    return std::nullopt;
  }
  return CalculateSha1Hex(image.subspan(kINESHeaderSize, content_size));
}

std::optional<std::string> CalculateINESBatterySaveSha1Hex(
    std::span<const uint8_t> image) {
  std::optional<std::string> sha1 = CalculateINESRomSha1Hex(image);
  if (!sha1 || (image[6] & 0x02) == 0) {
    return std::nullopt;
  }
  return sha1;
}

}  // namespace nes
}  // namespace kiwi
