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

#ifndef UTILITY_TEXTURE_PARSER_TEXTURE_PARSER_H_
#define UTILITY_TEXTURE_PARSER_TEXTURE_PARSER_H_

#include <cstdint>
#include <span>

enum class TextureType {
  kMesen,
};

class TextureParser {
 public:
  TextureParser() = default;
  virtual ~TextureParser() = default;

  TextureParser(const TextureParser&) = delete;
  TextureParser& operator=(const TextureParser&) = delete;

  virtual TextureType type() const = 0;

  // Verifies that the texture pack supports |rom_data|. Implementations must
  // not load or decode texture resources during this call.
  virtual bool Verify(std::span<const uint8_t> rom_data) const = 0;
};

#endif  // UTILITY_TEXTURE_PARSER_TEXTURE_PARSER_H_
