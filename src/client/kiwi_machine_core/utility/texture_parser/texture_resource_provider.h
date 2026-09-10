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

#ifndef UTILITY_TEXTURE_PARSER_TEXTURE_RESOURCE_PROVIDER_H_
#define UTILITY_TEXTURE_PARSER_TEXTURE_RESOURCE_PROVIDER_H_

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include "nes/types.h"

// Provides format-neutral access to texture pack resources.
class TextureResourceProvider {
 public:
  TextureResourceProvider() = default;
  virtual ~TextureResourceProvider() = default;

  TextureResourceProvider(const TextureResourceProvider&) = delete;
  TextureResourceProvider& operator=(const TextureResourceProvider&) = delete;

  virtual const std::unordered_set<std::string>& GetFilePaths() const = 0;
  virtual std::optional<kiwi::nes::Bytes> ReadFile(std::string_view path) = 0;
};

#endif  // UTILITY_TEXTURE_PARSER_TEXTURE_RESOURCE_PROVIDER_H_
