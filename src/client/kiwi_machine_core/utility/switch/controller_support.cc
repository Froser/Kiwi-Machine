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

#include "utility/switch/controller_support.h"

#include <switch.h>

namespace kiwi::switch_platform {

bool ShowControllerSupport(int min_players, int max_players) {
  if (min_players < 0 || max_players < 1 || min_players > max_players ||
      max_players > 8) {
    return false;
  }

  HidLaControllerSupportArg arg;
  hidLaCreateControllerSupportArg(&arg);
  arg.hdr.player_count_min = static_cast<s8>(min_players);
  arg.hdr.player_count_max = static_cast<s8>(max_players);

  HidLaControllerSupportResultInfo result_info = {};
  return R_SUCCEEDED(
      hidLaShowControllerSupportForSystem(&result_info, &arg, false));
}

}  // namespace kiwi::switch_platform
