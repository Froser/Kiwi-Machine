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

#include "emulator/gba/emulator.h"

#include "base/memory/scoped_refptr.h"
#include "emulator/gba/emulator_impl.h"

namespace kiwi {
namespace gba {

Emulator::Emulator() : emu::Emulator(emu::Emulator::Type::kGBA) {}
Emulator::~Emulator() = default;

scoped_refptr<Emulator> CreateEmulator() {
  return base::MakeRefCounted<EmulatorImpl>();
}

scoped_refptr<Emulator> CreateEmulatorForTesting() {
  return CreateEmulator();
}

}  // namespace gba
}  // namespace kiwi
