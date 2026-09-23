// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper113.h"

namespace kiwi {
namespace nes {

Mapper113::Mapper113(Cartridge* cartridge) : Mapper079(cartridge, true) {}

Mapper113::~Mapper113() = default;

}  // namespace nes
}  // namespace kiwi
