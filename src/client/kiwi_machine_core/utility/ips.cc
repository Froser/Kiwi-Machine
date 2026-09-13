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

#include "utility/ips.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace {

bool ReadBigEndian(std::span<const uint8_t> data,
                   size_t byte_count,
                   size_t* cursor,
                   uint32_t* value) {
  if (*cursor > data.size() || data.size() - *cursor < byte_count) {
    return false;
  }

  *value = 0;
  for (size_t index = 0; index < byte_count; ++index) {
    *value = (*value << 8) | data[(*cursor)++];
  }
  return true;
}

}  // namespace

bool ApplyIpsPatch(std::span<const uint8_t> patch_data,
                   std::vector<uint8_t>* data) {
  constexpr std::string_view kHeader = "PATCH";
  constexpr std::string_view kEndMarker = "EOF";
  if (!data || patch_data.size() < kHeader.size() + kEndMarker.size() ||
      !std::equal(kHeader.begin(), kHeader.end(), patch_data.begin())) {
    return false;
  }

  std::vector<uint8_t> result = *data;
  size_t cursor = kHeader.size();
  while (cursor <= patch_data.size()) {
    if (patch_data.size() - cursor >= kEndMarker.size() &&
        std::equal(kEndMarker.begin(), kEndMarker.end(),
                   patch_data.begin() + cursor)) {
      cursor += kEndMarker.size();
      const size_t remaining = patch_data.size() - cursor;
      if (remaining == 3) {
        uint32_t truncated_size = 0;
        if (!ReadBigEndian(patch_data, 3, &cursor, &truncated_size)) {
          return false;
        }
        result.resize(truncated_size);
      } else if (remaining != 0) {
        return false;
      }
      data->swap(result);
      return true;
    }

    uint32_t write_offset = 0;
    uint32_t write_size = 0;
    if (!ReadBigEndian(patch_data, 3, &cursor, &write_offset) ||
        !ReadBigEndian(patch_data, 2, &cursor, &write_size)) {
      return false;
    }

    if (write_size == 0) {
      uint32_t run_size = 0;
      if (!ReadBigEndian(patch_data, 2, &cursor, &run_size) ||
          run_size == 0 || cursor >= patch_data.size()) {
        return false;
      }
      const uint8_t value = patch_data[cursor++];
      const size_t write_end = static_cast<size_t>(write_offset) + run_size;
      if (result.size() < write_end) {
        result.resize(write_end);
      }
      std::fill(result.begin() + write_offset, result.begin() + write_end,
                value);
      continue;
    }

    if (cursor > patch_data.size() ||
        patch_data.size() - cursor < write_size) {
      return false;
    }
    const size_t write_end = static_cast<size_t>(write_offset) + write_size;
    if (result.size() < write_end) {
      result.resize(write_end);
    }
    std::copy_n(patch_data.begin() + cursor, write_size,
                result.begin() + write_offset);
    cursor += write_size;
  }
  return false;
}
