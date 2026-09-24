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

#ifndef NES_MAPPERS_MAPPER118_H_
#define NES_MAPPERS_MAPPER118_H_

#include <array>

#include "nes/mappers/mapper004.h"

namespace kiwi {
namespace nes {

// Nintendo TxSROM: MMC3 with CIRAM A10 selected by CHR bank bit 7.
// https://www.nesdev.org/wiki/INES_Mapper_118
class Mapper118 : public Mapper004 {
 public:
  explicit Mapper118(Cartridge* cartridge);
  ~Mapper118() override;

  void WritePRG(Address address, Byte value) override;

  bool UsesCustomPPUMemoryMapping() const override;
  Byte ReadPPUMemoryByte(Byte* ciram, Address address) override;
  void WritePPUMemoryByte(Byte* ciram, Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void UpdateNametablePages(Byte value);

  std::array<Byte, 4> nametable_pages_{};
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER118_H_
