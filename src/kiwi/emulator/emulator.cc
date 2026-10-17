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

#include "emulator/emulator.h"

#include "base/check.h"
#include "emulator/gba/emulator.h"
#include "emulator/nes/emulator.h"

namespace kiwi {
namespace emu {

Emulator::Emulator(Type type) : type_(type) {}
Emulator::~Emulator() = default;

nes::Emulator* Emulator::AsNESEmulator() {
  CHECK(GetType() == Type::kNES);
  return static_cast<nes::Emulator*>(this);
}

const nes::Emulator* Emulator::AsNESEmulator() const {
  CHECK(GetType() == Type::kNES);
  return static_cast<const nes::Emulator*>(this);
}

gba::Emulator* Emulator::AsGBAEmulator() {
  CHECK(GetType() == Type::kGBA);
  return static_cast<gba::Emulator*>(this);
}

const gba::Emulator* Emulator::AsGBAEmulator() const {
  CHECK(GetType() == Type::kGBA);
  return static_cast<const gba::Emulator*>(this);
}

}  // namespace emu
}  // namespace kiwi
