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

#ifndef UTILITY_IPS_H_
#define UTILITY_IPS_H_

#include <cstdint>
#include <span>
#include <vector>

// Applies an IPS patch transactionally. On success, |data| contains the
// patched result and may have been resized. On failure, |data| is unchanged.
bool ApplyIpsPatch(std::span<const uint8_t> patch_data,
                   std::vector<uint8_t>* data);

#endif  // UTILITY_IPS_H_
