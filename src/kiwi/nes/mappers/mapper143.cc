// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper143.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {
namespace {

constexpr Address kProtectionMask = 0xe100;
constexpr Address kProtectionBase = 0x4100;

}  // namespace

Mapper143::Mapper143(Cartridge* cartridge) : Mapper000(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 143);
}

Mapper143::~Mapper143() = default;

void Mapper143::WritePRG(Address address, Byte value) {}

Byte Mapper143::ReadExtendedRAM(Address address) {
  const Byte open_bus = Mapper::ReadExtendedRAM(address);
  if ((address & kProtectionMask) != kProtectionBase)
    return open_bus;

  return static_cast<Byte>((open_bus & 0xc0) | ((~address) & 0x3f));
}

}  // namespace nes
}  // namespace kiwi
