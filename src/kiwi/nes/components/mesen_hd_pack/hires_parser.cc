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

#include "nes/components/mesen_hd_pack/hires_parser.h"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace kiwi {
namespace nes {
namespace mesen_hd_pack {
namespace {

constexpr char kUtf8Bom[] = "\xef\xbb\xbf";

std::string_view Trim(std::string_view value) {
  const size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    return {};
  }
  const size_t last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::vector<std::string_view> Split(std::string_view value, char separator) {
  std::vector<std::string_view> result;
  size_t start = 0;
  while (start <= value.size()) {
    const size_t end = value.find(separator, start);
    if (end == std::string_view::npos) {
      result.push_back(Trim(value.substr(start)));
      break;
    }
    result.push_back(Trim(value.substr(start, end - start)));
    start = end + 1;
  }
  return result;
}

bool ParseUnsignedDecimal(std::string_view text, uint32_t* value) {
  text = Trim(text);
  if (text.empty()) {
    return false;
  }

  uint32_t parsed = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto result = std::from_chars(begin, end, parsed, 10);
  if (result.ec != std::errc() || result.ptr != end) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ParseSignedDecimal(std::string_view text, int32_t* value) {
  text = Trim(text);
  if (text.empty()) {
    return false;
  }

  int32_t parsed = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto result = std::from_chars(begin, end, parsed, 10);
  if (result.ec != std::errc() || result.ptr != end) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ParseHex(std::string_view text, uint32_t* value) {
  text = Trim(text);
  if (text.empty() || text.size() > 8) {
    return false;
  }

  uint32_t parsed = 0;
  const char* begin = text.data();
  const char* end = begin + text.size();
  const auto result = std::from_chars(begin, end, parsed, 16);
  if (result.ec != std::errc() || result.ptr != end) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ParseFloat(std::string_view text, float* value) {
  const std::string input(Trim(text));
  if (input.empty()) {
    return false;
  }

  char* end = nullptr;
  const float parsed = std::strtof(input.c_str(), &end);
  if (end != input.c_str() + input.size() || !std::isfinite(parsed)) {
    return false;
  }

  *value = parsed;
  return true;
}

bool ParseBoolean(std::string_view text, bool* value) {
  text = Trim(text);
  if (text == "Y") {
    *value = true;
    return true;
  }
  if (text == "N") {
    *value = false;
    return true;
  }
  return false;
}

template <size_t Size>
bool ParseHexBytes(std::string_view text, std::array<uint8_t, Size>* bytes) {
  text = Trim(text);
  if (text.size() != Size * 2) {
    return false;
  }

  for (size_t i = 0; i < Size; ++i) {
    uint32_t value = 0;
    if (!ParseHex(text.substr(i * 2, 2), &value)) {
      return false;
    }
    (*bytes)[i] = static_cast<uint8_t>(value);
  }
  return true;
}

bool IsHexString(std::string_view text) {
  for (char character : text) {
    const bool is_digit = character >= '0' && character <= '9';
    const bool is_lower = character >= 'a' && character <= 'f';
    const bool is_upper = character >= 'A' && character <= 'F';
    if (!is_digit && !is_lower && !is_upper) {
      return false;
    }
  }
  return !text.empty();
}

std::string Uppercase(std::string_view text) {
  std::string result(text);
  for (char& character : result) {
    if (character >= 'a' && character <= 'f') {
      character = static_cast<char>(character - 'a' + 'A');
    }
  }
  return result;
}

bool NormalizeResourcePath(std::string_view input, std::string* output) {
  input = Trim(input);
  if (input.empty() || input.front() == '/' || input.front() == '\\') {
    return false;
  }

  output->clear();
  std::string component;
  auto append_component = [&]() {
    if (component.empty() || component == "." || component == "..") {
      return false;
    }
    if (component.find(':') != std::string::npos) {
      return false;
    }
    if (!output->empty()) {
      output->push_back('/');
    }
    output->append(component);
    component.clear();
    return true;
  };

  for (char character : input) {
    if (character == '/' || character == '\\') {
      if (!append_component()) {
        return false;
      }
    } else {
      component.push_back(character);
    }
  }
  return append_component();
}

class ParserImpl {
 public:
  ParserImpl(HdPackData* data, std::vector<ParseError>* errors)
      : data_(data), errors_(errors) {}

  bool Parse(std::string_view contents) {
    AddBuiltInConditions();

    size_t position = 0;
    while (position <= contents.size()) {
      const size_t line_end = contents.find('\n', position);
      std::string_view line =
          line_end == std::string_view::npos
              ? contents.substr(position)
              : contents.substr(position, line_end - position);
      ++line_number_;

      if (line_number_ == 1 && line.starts_with(kUtf8Bom)) {
        line.remove_prefix(3);
      }
      ParseLine(line);

      if (line_end == std::string_view::npos) {
        break;
      }
      position = line_end + 1;
    }

    return errors_->empty();
  }

 private:
  void AddError(const std::string& message) {
    errors_->push_back({line_number_, message});
  }

  void AddBuiltInConditions() {
    AddCondition(
        {"hmirror", ConditionType::kHorizontalMirror, std::monostate()});
    AddCondition({"vmirror", ConditionType::kVerticalMirror, std::monostate()});
    AddCondition(
        {"bgpriority", ConditionType::kBackgroundPriority, std::monostate()});
  }

  void AddVersionedBuiltInConditions() {
    if (added_sprite_palette_conditions_ || data_->version < 107) {
      return;
    }

    for (uint8_t palette = 0; palette < 4; ++palette) {
      AddCondition({"sppalette" + std::to_string(palette),
                    ConditionType::kSpritePalette,
                    SpritePaletteConditionData{palette}});
    }
    added_sprite_palette_conditions_ = true;
  }

  void AddCondition(Condition condition) {
    const size_t index = data_->conditions.size();
    condition_indices_[condition.name] = index;
    data_->conditions.push_back(std::move(condition));
  }

  bool RequireVersion(uint32_t version, std::string_view feature) {
    if (data_->version >= version) {
      return true;
    }
    AddError(std::string(feature) + " requires hires.txt version " +
             std::to_string(version) + " or newer");
    return false;
  }

  bool RequireTokenCount(const std::vector<std::string_view>& tokens,
                         size_t minimum,
                         size_t maximum,
                         std::string_view tag) {
    if (tokens.size() < minimum || tokens.size() > maximum) {
      AddError(std::string(tag) + " has an invalid parameter count");
      return false;
    }
    return true;
  }

  bool ParsePath(std::string_view token,
                 std::string_view field_name,
                 std::string* path) {
    if (!NormalizeResourcePath(token, path)) {
      AddError("Invalid " + std::string(field_name) + " path");
      return false;
    }
    return true;
  }

  bool ParseUInt(std::string_view token,
                 std::string_view field_name,
                 uint32_t* value) {
    if (!ParseUnsignedDecimal(token, value)) {
      AddError("Invalid " + std::string(field_name));
      return false;
    }
    return true;
  }

  bool ParseInt(std::string_view token,
                std::string_view field_name,
                int32_t* value) {
    if (!ParseSignedDecimal(token, value)) {
      AddError("Invalid " + std::string(field_name));
      return false;
    }
    return true;
  }

  bool ParseHexValue(std::string_view token,
                     std::string_view field_name,
                     uint32_t* value) {
    if (!ParseHex(token, value)) {
      AddError("Invalid " + std::string(field_name));
      return false;
    }
    return true;
  }

  bool ParseFloatValue(std::string_view token,
                       std::string_view field_name,
                       float* value) {
    if (!ParseFloat(token, value)) {
      AddError("Invalid " + std::string(field_name));
      return false;
    }
    return true;
  }

  bool ParseBool(std::string_view token,
                 std::string_view field_name,
                 bool* value) {
    if (!ParseBoolean(token, value)) {
      AddError("Invalid " + std::string(field_name) + "; expected Y or N");
      return false;
    }
    return true;
  }

  bool ParseTileKey(std::string_view tile_token,
                    std::string_view palette_token,
                    uint32_t hexadecimal_version,
                    TileKey* tile) {
    if (!ParseHexBytes(palette_token, &tile->palette)) {
      AddError("Tile palette must contain exactly four hexadecimal bytes");
      return false;
    }

    tile_token = Trim(tile_token);
    if (tile_token.size() == kChrTileByteCount * 2) {
      tile->source = TileDataSource::kChrRam;
      if (!ParseHexBytes(tile_token, &tile->chr_data)) {
        AddError("CHR RAM tile data must contain 16 hexadecimal bytes");
        return false;
      }
      return true;
    }

    tile->source = TileDataSource::kChrRom;
    const bool parsed =
        data_->version >= hexadecimal_version
            ? ParseHex(tile_token, &tile->chr_rom_index)
            : ParseUnsignedDecimal(tile_token, &tile->chr_rom_index);
    if (!parsed) {
      AddError("Invalid CHR ROM tile index");
      return false;
    }
    return true;
  }

  bool ParseComparison(std::string_view token, ComparisonOperator* comparison) {
    token = Trim(token);
    if (token == "==") {
      *comparison = ComparisonOperator::kEqual;
    } else if (token == "!=") {
      *comparison = ComparisonOperator::kNotEqual;
    } else if (token == ">") {
      *comparison = ComparisonOperator::kGreaterThan;
    } else if (token == "<") {
      *comparison = ComparisonOperator::kLessThan;
    } else if (token == ">=") {
      *comparison = ComparisonOperator::kGreaterThanOrEqual;
    } else if (token == "<=") {
      *comparison = ComparisonOperator::kLessThanOrEqual;
    } else {
      AddError("Invalid comparison operator");
      return false;
    }
    return true;
  }

  std::vector<ConditionRef> ParseConditionRefs(std::string_view text) {
    std::vector<ConditionRef> result;
    for (std::string_view token : Split(text, '&')) {
      bool negated = false;
      if (token.starts_with('!')) {
        negated = true;
        token.remove_prefix(1);
        token = Trim(token);
      }

      const auto condition = condition_indices_.find(std::string(token));
      if (token.empty() || condition == condition_indices_.end()) {
        AddError("Unknown condition in rule: " + std::string(token));
        continue;
      }
      result.push_back({condition->second, negated});
    }
    return result;
  }

  void ParseLine(std::string_view line) {
    line = Trim(line);
    if (line.empty() || line.starts_with('#')) {
      return;
    }

    std::vector<ConditionRef> conditions;
    if (line.starts_with('[')) {
      const size_t end = line.find(']');
      if (end == std::string_view::npos) {
        AddError("Condition prefix is missing a closing ']'");
        return;
      }
      conditions = ParseConditionRefs(line.substr(1, end - 1));
      line = Trim(line.substr(end + 1));
    }

    if (!line.starts_with('<')) {
      return;
    }
    const size_t tag_end = line.find('>');
    if (tag_end == std::string_view::npos) {
      AddError("Tag is missing a closing '>'");
      return;
    }

    const std::string_view tag = line.substr(1, tag_end - 1);
    const std::string_view payload = Trim(line.substr(tag_end + 1));
    const std::vector<std::string_view> tokens = Split(payload, ',');

    if (tag == "ver") {
      ParseVersion(payload);
    } else if (tag == "scale") {
      ParseScale(payload);
    } else if (tag == "supportedRom") {
      ParseSupportedRom(payload);
    } else if (tag == "overscan") {
      ParseOverscan(tokens);
    } else if (tag == "patch") {
      ParsePatch(tokens);
    } else if (tag == "img") {
      ParseImage(payload);
    } else if (tag == "condition") {
      ParseCondition(tokens);
    } else if (tag == "tile") {
      ParseTile(tokens, std::move(conditions));
    } else if (tag == "background") {
      ParseBackground(tokens, std::move(conditions));
    } else if (tag == "addition") {
      ParseAddition(tokens);
    } else if (tag == "fallback") {
      ParseFallback(tokens);
    } else if (tag == "options") {
      ParseOptions(tokens);
    } else if (tag == "bgm") {
      ParseAudio(tokens, true);
    } else if (tag == "sfx") {
      ParseAudio(tokens, false);
    }
  }

  void ParseVersion(std::string_view payload) {
    uint32_t version = 0;
    if (!ParseUInt(payload, "format version", &version)) {
      return;
    }
    if (version > kCurrentHiresVersion) {
      AddError("hires.txt version " + std::to_string(version) +
               " is newer than the supported version " +
               std::to_string(kCurrentHiresVersion));
    }
    data_->version = version;
    AddVersionedBuiltInConditions();
  }

  void ParseScale(std::string_view payload) {
    uint32_t scale = 0;
    if (!ParseUInt(payload, "scale", &scale)) {
      return;
    }
    if (scale == 0 || scale > 10) {
      AddError("Scale must be between 1 and 10");
      return;
    }
    data_->scale = scale;
  }

  void ParseSupportedRom(std::string_view payload) {
    payload = Trim(payload);
    if (payload.size() != 40 || !IsHexString(payload)) {
      AddError("supportedRom must be a 40-character SHA-1 hash");
      return;
    }
    data_->supported_rom_sha1s.push_back(Uppercase(payload));
  }

  void ParseOverscan(const std::vector<std::string_view>& tokens) {
    if (!RequireTokenCount(tokens, 4, 4, "overscan")) {
      return;
    }

    Overscan overscan;
    if (!ParseInt(tokens[0], "top overscan", &overscan.top) ||
        !ParseInt(tokens[1], "right overscan", &overscan.right) ||
        !ParseInt(tokens[2], "bottom overscan", &overscan.bottom) ||
        !ParseInt(tokens[3], "left overscan", &overscan.left)) {
      return;
    }
    data_->overscan = overscan;
  }

  void ParsePatch(const std::vector<std::string_view>& tokens) {
    if (!RequireTokenCount(tokens, 2, 2, "patch")) {
      return;
    }

    PatchRule patch;
    if (!ParsePath(tokens[0], "patch", &patch.file)) {
      return;
    }
    const std::string_view sha1 = Trim(tokens[1]);
    if (sha1.size() != 40 || !IsHexString(sha1)) {
      AddError("Patch SHA-1 must contain 40 hexadecimal characters");
      return;
    }
    patch.rom_sha1 = Uppercase(sha1);
    data_->patches.push_back(std::move(patch));
  }

  void ParseImage(std::string_view payload) {
    std::string image;
    if (ParsePath(payload, "image", &image)) {
      data_->image_files.push_back(std::move(image));
    }
  }

  void ParseCondition(const std::vector<std::string_view>& tokens) {
    if (tokens.size() < 2) {
      AddError("condition has an invalid parameter count");
      return;
    }

    const std::string name(Trim(tokens[0]));
    const std::string_view type = Trim(tokens[1]);
    if (name.empty() || name.find_first_of("!&[]") != std::string::npos) {
      AddError("Condition name is empty or contains a reserved character");
      return;
    }

    if (type == "tileAtPosition" || type == "tileNearby" ||
        type == "spriteAtPosition" || type == "spriteNearby") {
      ParseTileCondition(name, type, tokens);
    } else if (type == "memoryCheck" || type == "ppuMemoryCheck" ||
               type == "memoryCheckConstant" ||
               type == "ppuMemoryCheckConstant") {
      ParseMemoryCondition(name, type, tokens);
    } else if (type == "frameRange") {
      ParseFrameRangeCondition(name, tokens);
    } else if (type == "positionCheckX" || type == "positionCheckY" ||
               type == "originPositionCheckX" ||
               type == "originPositionCheckY") {
      ParsePositionCondition(name, type, tokens);
    } else {
      AddError("Unknown condition type: " + std::string(type));
    }
  }

  void ParseTileCondition(const std::string& name,
                          std::string_view type,
                          const std::vector<std::string_view>& tokens) {
    if (!RequireTokenCount(tokens, 6, 7, "condition")) {
      return;
    }

    TileConditionData condition_data;
    if (!ParseInt(tokens[2], "condition X coordinate", &condition_data.x) ||
        !ParseInt(tokens[3], "condition Y coordinate", &condition_data.y) ||
        !ParseTileKey(tokens[4], tokens[5], 104, &condition_data.tile)) {
      return;
    }

    if (tokens.size() == 7) {
      if (!RequireVersion(108, "Condition palette override") ||
          !ParseBool(tokens[6], "condition ignore-palette flag",
                     &condition_data.ignore_palette)) {
        return;
      }
    }

    ConditionType condition_type;
    if (type == "tileAtPosition") {
      condition_type = ConditionType::kTileAtPosition;
    } else if (type == "tileNearby") {
      condition_type = ConditionType::kTileNearby;
    } else if (type == "spriteAtPosition") {
      condition_type = ConditionType::kSpriteAtPosition;
    } else {
      condition_type = ConditionType::kSpriteNearby;
    }
    AddCondition({name, condition_type, std::move(condition_data)});
  }

  void ParseMemoryCondition(const std::string& name,
                            std::string_view type,
                            const std::vector<std::string_view>& tokens) {
    if (!RequireVersion(101, "Memory condition") ||
        !RequireTokenCount(tokens, 5, 6, "condition")) {
      return;
    }

    MemoryConditionData condition_data;
    if (!ParseHexValue(tokens[2], "memory address",
                       &condition_data.left_operand) ||
        !ParseComparison(tokens[3], &condition_data.comparison) ||
        !ParseHexValue(tokens[4], "memory comparison operand",
                       &condition_data.right_operand)) {
      return;
    }

    const bool uses_ppu_memory = type.starts_with("ppu");
    const bool compares_constant = type.ends_with("Constant");
    const uint32_t maximum_address = uses_ppu_memory ? 0x3fff : 0xffff;
    if (condition_data.left_operand > maximum_address) {
      AddError("Memory condition address is out of range");
      return;
    }
    if (compares_constant) {
      if (condition_data.right_operand > 0xff) {
        AddError("Memory condition constant is out of range");
        return;
      }
    } else if (condition_data.right_operand > maximum_address) {
      AddError("Memory condition comparison address is out of range");
      return;
    }

    if (tokens.size() == 6) {
      if (!RequireVersion(103, "Memory condition mask")) {
        return;
      }
      uint32_t mask = 0;
      if (!ParseHexValue(tokens[5], "memory condition mask", &mask) ||
          mask > 0xff) {
        AddError("Memory condition mask is out of range");
        return;
      }
      condition_data.mask = static_cast<uint8_t>(mask);
    }

    ConditionType condition_type;
    if (type == "memoryCheck") {
      condition_type = ConditionType::kMemoryCheck;
    } else if (type == "ppuMemoryCheck") {
      condition_type = ConditionType::kPpuMemoryCheck;
    } else if (type == "memoryCheckConstant") {
      condition_type = ConditionType::kMemoryCheckConstant;
    } else {
      condition_type = ConditionType::kPpuMemoryCheckConstant;
    }
    AddCondition({name, condition_type, std::move(condition_data)});
  }

  void ParseFrameRangeCondition(const std::string& name,
                                const std::vector<std::string_view>& tokens) {
    if (!RequireVersion(101, "frameRange condition") ||
        !RequireTokenCount(tokens, 4, 4, "condition")) {
      return;
    }

    FrameRangeConditionData condition_data;
    const bool parsed =
        data_->version == 101
            ? ParseHexValue(tokens[2], "frame divisor",
                            &condition_data.divisor) &&
                  ParseHexValue(tokens[3], "frame comparison value",
                                &condition_data.compare_value)
            : ParseUInt(tokens[2], "frame divisor", &condition_data.divisor) &&
                  ParseUInt(tokens[3], "frame comparison value",
                            &condition_data.compare_value);
    if (!parsed) {
      return;
    }
    if (condition_data.divisor == 0 ||
        condition_data.divisor > std::numeric_limits<uint16_t>::max() ||
        condition_data.compare_value > std::numeric_limits<uint16_t>::max()) {
      AddError("frameRange operands are out of range");
      return;
    }

    AddCondition({name, ConditionType::kFrameRange, std::move(condition_data)});
  }

  void ParsePositionCondition(const std::string& name,
                              std::string_view type,
                              const std::vector<std::string_view>& tokens) {
    if (!RequireVersion(108, "Position condition") ||
        !RequireTokenCount(tokens, 4, 4, "condition")) {
      return;
    }

    PositionConditionData condition_data;
    if (!ParseComparison(tokens[2], &condition_data.comparison) ||
        !ParseUInt(tokens[3], "position", &condition_data.position)) {
      return;
    }
    if (condition_data.position > std::numeric_limits<uint16_t>::max()) {
      AddError("Position condition operand is out of range");
      return;
    }

    ConditionType condition_type;
    if (type == "positionCheckX") {
      condition_type = ConditionType::kPositionCheckX;
    } else if (type == "positionCheckY") {
      condition_type = ConditionType::kPositionCheckY;
    } else if (type == "originPositionCheckX") {
      condition_type = ConditionType::kOriginPositionCheckX;
    } else {
      condition_type = ConditionType::kOriginPositionCheckY;
    }
    AddCondition({name, condition_type, std::move(condition_data)});
  }

  void ParseTile(const std::vector<std::string_view>& tokens,
                 std::vector<ConditionRef> conditions) {
    if (data_->version < 100) {
      ParseLegacyTile(tokens, std::move(conditions));
      return;
    }
    if (!RequireTokenCount(tokens, 7, 9, "tile")) {
      return;
    }

    TileRule tile;
    uint32_t image_index = 0;
    if (!ParseUInt(tokens[0], "tile image index", &image_index) ||
        !ParseTileKey(tokens[1], tokens[2], 103, &tile.tile) ||
        !ParseUInt(tokens[3], "tile image X coordinate", &tile.image_x) ||
        !ParseUInt(tokens[4], "tile image Y coordinate", &tile.image_y) ||
        !ParseFloatValue(tokens[5], "tile brightness", &tile.brightness) ||
        !ParseBool(tokens[6], "tile default flag", &tile.default_for_tile)) {
      return;
    }
    if (image_index >= data_->image_files.size()) {
      AddError("Tile references an image index that has not been declared");
      return;
    }
    tile.image_index = image_index;
    tile.conditions = std::move(conditions);

    if (tile.tile.source == TileDataSource::kChrRom && tokens.size() != 7) {
      AddError("CHR ROM tile contains CHR RAM-only parameters");
      return;
    }
    if (tile.tile.source == TileDataSource::kChrRam && tokens.size() >= 8) {
      uint32_t bank_id = 0;
      if (!ParseUInt(tokens[7], "CHR RAM bank ID", &bank_id)) {
        return;
      }
      tile.chr_ram_bank_id = bank_id;
    }
    if (tile.tile.source == TileDataSource::kChrRam && tokens.size() == 9) {
      uint32_t tile_index = 0;
      if (!ParseUInt(tokens[8], "CHR RAM tile index", &tile_index)) {
        return;
      }
      tile.chr_ram_tile_index = tile_index;
    }
    data_->tiles.push_back(std::move(tile));
  }

  void ParseLegacyTile(const std::vector<std::string_view>& tokens,
                       std::vector<ConditionRef> conditions) {
    const size_t base_count = data_->version == 0 ? 8 : 9;
    if (tokens.size() != base_count &&
        tokens.size() != base_count + kChrTileByteCount) {
      AddError("Legacy tile has an invalid parameter count");
      return;
    }

    TileRule tile;
    uint32_t image_index = 0;
    uint32_t tile_index = 0;
    uint32_t palette_values[3] = {};
    if (!ParseUInt(tokens[0], "legacy tile index", &tile_index) ||
        !ParseUInt(tokens[1], "legacy image index", &image_index) ||
        !ParseUInt(tokens[2], "legacy palette color", &palette_values[0]) ||
        !ParseUInt(tokens[3], "legacy palette color", &palette_values[1]) ||
        !ParseUInt(tokens[4], "legacy palette color", &palette_values[2]) ||
        !ParseUInt(tokens[5], "tile image X coordinate", &tile.image_x) ||
        !ParseUInt(tokens[6], "tile image Y coordinate", &tile.image_y)) {
      return;
    }
    if (image_index >= data_->image_files.size()) {
      AddError("Tile references an image index that has not been declared");
      return;
    }
    for (uint32_t color : palette_values) {
      if (color > 0xff) {
        AddError("Legacy palette color is out of range");
        return;
      }
    }

    size_t index = 7;
    if (data_->version > 0 &&
        !ParseFloatValue(tokens[index++], "tile brightness",
                         &tile.brightness)) {
      return;
    }
    if (!ParseBool(tokens[index++], "tile default flag",
                   &tile.default_for_tile)) {
      return;
    }

    tile.image_index = image_index;
    tile.tile.source = TileDataSource::kChrRom;
    tile.tile.chr_rom_index = tile_index;
    tile.tile.palette = {0, static_cast<uint8_t>(palette_values[0]),
                         static_cast<uint8_t>(palette_values[1]),
                         static_cast<uint8_t>(palette_values[2])};
    tile.conditions = std::move(conditions);

    if (tokens.size() == base_count + kChrTileByteCount) {
      tile.tile.source = TileDataSource::kChrRam;
      for (size_t byte = 0; byte < kChrTileByteCount; ++byte) {
        uint32_t value = 0;
        if (!ParseUInt(tokens[index++], "legacy CHR RAM byte", &value) ||
            value > 0xff) {
          AddError("Legacy CHR RAM byte is out of range");
          return;
        }
        tile.tile.chr_data[byte] = static_cast<uint8_t>(value);
      }
    }
    data_->tiles.push_back(std::move(tile));
  }

  void ParseBackground(const std::vector<std::string_view>& tokens,
                       std::vector<ConditionRef> conditions) {
    if (!RequireTokenCount(tokens, 2, 8, "background")) {
      return;
    }

    BackgroundRule background;
    if (!ParsePath(tokens[0], "background image", &background.image_file) ||
        !ParseFloatValue(tokens[1], "background brightness",
                         &background.brightness)) {
      return;
    }
    if (tokens.size() > 2) {
      if ((data_->version != 2 &&
           !RequireVersion(101, "Background scrolling")) ||
          !ParseFloatValue(tokens[2], "horizontal scroll ratio",
                           &background.horizontal_scroll_ratio)) {
        return;
      }
    }
    if (tokens.size() > 3 &&
        !ParseFloatValue(tokens[3], "vertical scroll ratio",
                         &background.vertical_scroll_ratio)) {
      return;
    }
    if (tokens.size() > 4) {
      // Legacy version 2 packs use the same boolean priority field as
      // versions 102-105 while retaining their legacy tile rule syntax.
      if (data_->version != 2 && !RequireVersion(102, "Background priority")) {
        return;
      }
      if (data_->version >= 106) {
        uint32_t priority = 0;
        if (!ParseUInt(tokens[4], "background priority", &priority) ||
            priority >= 40) {
          AddError("Background priority must be between 0 and 39");
          return;
        }
        background.priority = static_cast<uint8_t>(priority);
      } else {
        bool behind_all_layers = false;
        if (!ParseBool(tokens[4], "background priority", &behind_all_layers)) {
          return;
        }
        background.priority = behind_all_layers ? 0 : 10;
      }
    }
    if (tokens.size() == 6) {
      AddError("Background image offsets require both left and top values");
      return;
    }
    if (tokens.size() > 6) {
      int32_t image_left = 0;
      int32_t image_top = 0;
      if (!RequireVersion(105, "Background image offsets") ||
          !ParseInt(tokens[5], "background left offset", &image_left) ||
          !ParseInt(tokens[6], "background top offset", &image_top)) {
        return;
      }
      background.image_left =
          image_left > 0 ? static_cast<uint32_t>(image_left) : 0;
      background.image_top =
          image_top > 0 ? static_cast<uint32_t>(image_top) : 0;
    }
    if (tokens.size() > 7) {
      if (!RequireVersion(107, "Background blend mode")) {
        return;
      }
      if (tokens[7] == "Alpha") {
        background.blend_mode = BackgroundBlendMode::kAlpha;
      } else if (tokens[7] == "Add") {
        background.blend_mode = BackgroundBlendMode::kAdd;
      } else if (tokens[7] == "Subtract") {
        background.blend_mode = BackgroundBlendMode::kSubtract;
      } else {
        AddError("Invalid background blend mode");
        return;
      }
    }

    for (const ConditionRef& condition_ref : conditions) {
      const ConditionType type =
          data_->conditions[condition_ref.condition_index].type;
      if (type != ConditionType::kTileAtPosition &&
          type != ConditionType::kSpriteAtPosition &&
          type != ConditionType::kMemoryCheck &&
          type != ConditionType::kPpuMemoryCheck &&
          type != ConditionType::kMemoryCheckConstant &&
          type != ConditionType::kPpuMemoryCheckConstant &&
          type != ConditionType::kFrameRange) {
        AddError("Condition type cannot be used by a background rule");
        return;
      }
    }
    background.conditions = std::move(conditions);
    data_->backgrounds.push_back(std::move(background));
  }

  void ParseAddition(const std::vector<std::string_view>& tokens) {
    if (!RequireVersion(107, "addition") ||
        !RequireTokenCount(tokens, 6, 7, "addition")) {
      return;
    }

    AdditionalSpriteRule addition;
    if (!ParseTileKey(tokens[0], tokens[1], 103, &addition.original_tile) ||
        !ParseInt(tokens[2], "additional sprite X offset",
                  &addition.offset_x) ||
        !ParseInt(tokens[3], "additional sprite Y offset",
                  &addition.offset_y) ||
        !ParseTileKey(tokens[4], tokens[5], 103, &addition.additional_tile)) {
      return;
    }
    if (tokens.size() == 7 &&
        (!RequireVersion(108, "Addition palette override") ||
         !ParseBool(tokens[6], "addition ignore-palette flag",
                    &addition.ignore_palette))) {
      return;
    }
    data_->additional_sprites.push_back(std::move(addition));
  }

  void ParseFallback(const std::vector<std::string_view>& tokens) {
    if (!RequireVersion(107, "fallback") ||
        !RequireTokenCount(tokens, 2, 2, "fallback")) {
      return;
    }

    FallbackRule fallback;
    if (!ParseHexValue(tokens[0], "fallback tile index",
                       &fallback.tile_index) ||
        !ParseHexValue(tokens[1], "fallback target tile index",
                       &fallback.fallback_tile_index)) {
      return;
    }
    data_->fallback_tiles.push_back(fallback);
  }

  void ParseOptions(const std::vector<std::string_view>& tokens) {
    for (std::string_view option : tokens) {
      uint32_t flag = 0;
      if (option == "disableSpriteLimit") {
        flag = static_cast<uint32_t>(HdPackOption::kDisableSpriteLimit);
      } else if (option == "alternateRegisterRange") {
        flag = static_cast<uint32_t>(HdPackOption::kAlternateRegisterRange);
      } else if (option == "disableCache") {
        flag = static_cast<uint32_t>(HdPackOption::kDisableCache);
      } else if (option == "disableOriginalTiles") {
        flag = static_cast<uint32_t>(HdPackOption::kDisableOriginalTiles);
      } else if (option == "automaticFallbackTiles") {
        flag = static_cast<uint32_t>(HdPackOption::kAutomaticFallbackTiles);
      } else if (option == "disableContours" || option.empty()) {
        continue;
      } else {
        AddError("Unknown HD Pack option: " + std::string(option));
        continue;
      }
      data_->options |= flag;
    }
  }

  void ParseAudio(const std::vector<std::string_view>& tokens, bool is_bgm) {
    const size_t maximum_tokens = is_bgm ? 4 : 3;
    if (!RequireTokenCount(tokens, 3, maximum_tokens, is_bgm ? "bgm" : "sfx")) {
      return;
    }

    uint32_t album = 0;
    uint32_t track = 0;
    AudioRule audio;
    if (!ParseUInt(tokens[0], "audio album", &album) || album > 0xff ||
        !ParseUInt(tokens[1], "audio track", &track) || track > 0xff ||
        !ParsePath(tokens[2], "audio file", &audio.file)) {
      if (album > 0xff || track > 0xff) {
        AddError("Audio album and track must be between 0 and 255");
      }
      return;
    }
    audio.album = static_cast<uint8_t>(album);
    audio.track = static_cast<uint8_t>(track);

    if (is_bgm && tokens.size() == 4) {
      uint32_t loop_position = 0;
      if (!ParseUInt(tokens[3], "BGM loop position", &loop_position)) {
        return;
      }
      audio.loop_position = loop_position;
    }

    if (is_bgm) {
      data_->bgm_tracks.push_back(std::move(audio));
    } else {
      data_->sfx_tracks.push_back(std::move(audio));
    }
  }

  HdPackData* data_;
  std::vector<ParseError>* errors_;
  std::unordered_map<std::string, size_t> condition_indices_;
  size_t line_number_ = 0;
  bool added_sprite_palette_conditions_ = false;
};

}  // namespace

bool HiresParser::Parse(std::string_view contents,
                        HdPackData* data,
                        std::vector<ParseError>* errors) const {
  if (!data || !errors) {
    return false;
  }

  *data = HdPackData();
  errors->clear();
  ParserImpl parser(data, errors);
  return parser.Parse(contents);
}

}  // namespace mesen_hd_pack
}  // namespace nes
}  // namespace kiwi
