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

#ifndef NES_MAPPERS_MAPPER119_H_
#define NES_MAPPERS_MAPPER119_H_

#include "nes/mappers/mapper004.h"

namespace kiwi {
namespace nes {

// Nintendo TQROM: MMC3 with CHR A16 selecting between CHR-ROM and CHR-RAM.
// https://www.nesdev.org/wiki/INES_Mapper_119
class Mapper119 : public Mapper004 {
 public:
  explicit Mapper119(Cartridge* cartridge);
  ~Mapper119() override;

  void WriteCHR(Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  Byte ReadCHRByBank(int bank, Address address) override;

 private:
  Bytes tqrom_character_ram_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER119_H_
