// Copyright (C) 2023 Yisi Yu
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

#include "nes/emulator.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "nes/emulator_impl.h"

namespace kiwi {
namespace nes {
Emulator::Emulator() = default;
Emulator::~Emulator() = default;

scoped_refptr<Emulator> CreateEmulator() {
  return base::MakeRefCounted<EmulatorImpl>();
}

}  // namespace nes
}  // namespace kiwi
