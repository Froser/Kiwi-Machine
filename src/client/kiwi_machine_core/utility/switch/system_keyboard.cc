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

#include "utility/switch/system_keyboard.h"

#include <switch.h>

#include <limits>
#include <vector>

namespace kiwi::switch_platform {

bool ShowSystemKeyboard(const std::string& title,
                        const std::string& confirm_text,
                        const std::string& initial_text,
                        size_t max_characters,
                        std::string* output) {
  if (!output || max_characters == 0 ||
      max_characters > std::numeric_limits<u32>::max() ||
      max_characters > (std::numeric_limits<size_t>::max() - 1) / 4) {
    return false;
  }

  SwkbdConfig config = {};
  if (R_FAILED(swkbdCreate(&config, 0)))
    return false;

  swkbdConfigMakePresetDefault(&config);
  swkbdConfigSetType(&config, SwkbdType_All);
  swkbdConfigSetReturnButtonFlag(&config, 0);
  swkbdConfigSetStringLenMax(&config, static_cast<u32>(max_characters));
  swkbdConfigSetHeaderText(&config, title.c_str());
  swkbdConfigSetGuideText(&config, title.c_str());
  swkbdConfigSetOkButtonText(&config, confirm_text.c_str());
  swkbdConfigSetInitialText(&config, initial_text.c_str());

  // A UTF-8 code point occupies at most four bytes.
  std::vector<char> result(max_characters * 4 + 1);
  const Result show_result =
      swkbdShow(&config, result.data(), result.size());
  swkbdClose(&config);
  if (R_FAILED(show_result))
    return false;

  *output = result.data();
  return true;
}

}  // namespace kiwi::switch_platform
