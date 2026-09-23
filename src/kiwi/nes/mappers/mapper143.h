// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER143_H_
#define NES_MAPPERS_MAPPER143_H_

#include "nes/mappers/mapper000.h"

namespace kiwi {
namespace nes {

// Sachen NROM board with address-based protection.
// https://www.nesdev.org/wiki/INES_Mapper_143
class Mapper143 : public Mapper000 {
 public:
  explicit Mapper143(Cartridge* cartridge);
  ~Mapper143() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER143_H_
