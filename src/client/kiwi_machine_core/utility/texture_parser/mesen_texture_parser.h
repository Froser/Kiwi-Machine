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

#ifndef UTILITY_TEXTURE_PARSER_MESEN_TEXTURE_PARSER_H_
#define UTILITY_TEXTURE_PARSER_MESEN_TEXTURE_PARSER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "nes/components/mesen_hd_pack/hd_pack_types.h"
#include "utility/texture_parser/texture_parser.h"

// Parses and verifies a Mesen texture pack, then creates its renderer.
class MesenTextureParser final : public TextureParser {
 public:
  MesenTextureParser(std::string archive_root,
                     std::string_view hires_contents,
                     const std::unordered_set<std::string>& archive_entries);
  ~MesenTextureParser() override = default;

  TextureType type() const override { return TextureType::kMesen; }
  bool Verify(std::span<const uint8_t> rom_data) const override;
  bool HasRomPatch(std::span<const uint8_t> rom_data) const override;
  bool ApplyRomPatch(std::vector<uint8_t>* rom_data,
                     TextureResourceProvider& resources) const override;
  std::unique_ptr<TextureRenderer> CreateTextureRenderer(
      std::span<const uint8_t> rom_data,
      TextureResourceProvider& resources) const override;

  bool definition_valid() const { return definition_valid_; }
  const std::vector<kiwi::nes::mesen_hd_pack::ParseError>& parse_errors()
      const {
    return parse_errors_;
  }

 private:
  bool HasRequiredResources(
      const std::unordered_set<std::string>& archive_entries) const;

  std::string archive_root_;
  kiwi::nes::mesen_hd_pack::HdPackData pack_data_;
  std::vector<kiwi::nes::mesen_hd_pack::ParseError> parse_errors_;
  bool definition_valid_ = false;
  bool resources_available_ = false;
};

class MesenTextureParserCollection final : public TextureParser {
 public:
  explicit MesenTextureParserCollection(
      std::vector<std::unique_ptr<MesenTextureParser>> parsers);
  ~MesenTextureParserCollection() override = default;

  TextureType type() const override { return TextureType::kMesen; }
  bool Verify(std::span<const uint8_t> rom_data) const override;
  bool HasRomPatch(std::span<const uint8_t> rom_data) const override;
  bool ApplyRomPatch(std::vector<uint8_t>* rom_data,
                     TextureResourceProvider& resources) const override;
  std::unique_ptr<TextureRenderer> CreateTextureRenderer(
      std::span<const uint8_t> rom_data,
      TextureResourceProvider& resources) const override;

  std::size_t size() const { return parsers_.size(); }

 private:
  std::vector<std::unique_ptr<MesenTextureParser>> parsers_;
};

#endif  // UTILITY_TEXTURE_PARSER_MESEN_TEXTURE_PARSER_H_
