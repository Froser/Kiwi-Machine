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

#include "utility/switch/system_language.h"

#include <switch.h>

#include <cstring>

namespace kiwi::switch_platform {

std::string GetSystemLanguage() {
  const Result initialize_result = setInitialize();
  if (R_FAILED(initialize_result))
    return std::string();

  u64 language_code = 0;
  const Result language_result = setGetSystemLanguage(&language_code);
  setExit();
  if (R_FAILED(language_result))
    return std::string();

  char language[sizeof(language_code) + 1] = {};
  std::memcpy(language, &language_code, sizeof(language_code));
  return language;
}

}  // namespace kiwi::switch_platform
