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

#include "utility/texture_parser/mesen_texture_parser.h"

#include <algorithm>
#include <set>
#include <utility>

#include "nes/components/mesen_hd_pack/hires_parser.h"
#include "nes/components/mesen_hd_pack/rom_hash.h"

namespace {

bool NormalizeArchiveRoot(std::string* archive_root) {
  std::replace(archive_root->begin(), archive_root->end(), '\\', '/');
  while (!archive_root->empty() && archive_root->back() == '/') {
    archive_root->pop_back();
  }
  if (archive_root->empty() || archive_root->front() == '/') {
    return false;
  }

  size_t component_start = 0;
  while (component_start <= archive_root->size()) {
    const size_t component_end = archive_root->find('/', component_start);
    const std::string_view component =
        component_end == std::string::npos
            ? std::string_view(*archive_root).substr(component_start)
            : std::string_view(*archive_root)
                  .substr(component_start, component_end - component_start);
    if (component.empty() || component == "." || component == "..") {
      return false;
    }
    if (component_end == std::string::npos) {
      break;
    }
    component_start = component_end + 1;
  }
  return true;
}

std::string JoinArchivePath(std::string_view root, std::string_view file) {
  std::string result(root);
  result.push_back('/');
  result.append(file);
  return result;
}

}  // namespace

MesenTextureParser::MesenTextureParser(
    std::string archive_root,
    std::string_view hires_contents,
    const std::unordered_set<std::string>& archive_entries)
    : archive_root_(std::move(archive_root)) {
  if (!NormalizeArchiveRoot(&archive_root_)) {
    parse_errors_.push_back({0, "Invalid Mesen HD Pack archive root"});
    return;
  }

  kiwi::nes::mesen_hd_pack::HiresParser parser;
  definition_valid_ = parser.Parse(hires_contents, &pack_data_, &parse_errors_);
  if (definition_valid_) {
    resources_available_ = HasRequiredResources(archive_entries);
  }
}

bool MesenTextureParser::Verify(std::span<const uint8_t> rom_data) const {
  if (!definition_valid_ || !resources_available_) {
    return false;
  }

  const std::string sha1 = kiwi::nes::mesen_hd_pack::CalculateSha1Hex(rom_data);
  return std::find(pack_data_.supported_rom_sha1s.begin(),
                   pack_data_.supported_rom_sha1s.end(),
                   sha1) != pack_data_.supported_rom_sha1s.end();
}

bool MesenTextureParser::HasRequiredResources(
    const std::unordered_set<std::string>& archive_entries) const {
  std::set<std::string> resources = {"hires.txt"};
  resources.insert(pack_data_.image_files.begin(),
                   pack_data_.image_files.end());
  for (const auto& background : pack_data_.backgrounds) {
    resources.insert(background.image_file);
  }
  for (const auto& patch : pack_data_.patches) {
    resources.insert(patch.file);
  }
  for (const auto& bgm : pack_data_.bgm_tracks) {
    resources.insert(bgm.file);
  }
  for (const auto& sfx : pack_data_.sfx_tracks) {
    resources.insert(sfx.file);
  }

  for (const std::string& resource : resources) {
    if (!archive_entries.contains(JoinArchivePath(archive_root_, resource))) {
      return false;
    }
  }
  return true;
}

MesenTextureParserCollection::MesenTextureParserCollection(
    std::vector<std::unique_ptr<MesenTextureParser>> parsers)
    : parsers_(std::move(parsers)) {}

bool MesenTextureParserCollection::Verify(
    std::span<const uint8_t> rom_data) const {
  return std::any_of(
      parsers_.begin(), parsers_.end(),
      [rom_data](const std::unique_ptr<MesenTextureParser>& parser) {
        return parser->Verify(rom_data);
      });
}
