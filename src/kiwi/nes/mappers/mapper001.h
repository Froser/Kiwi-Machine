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

#ifndef NES_MAPPERS_MAPPER001_H_
#define NES_MAPPERS_MAPPER001_H_

#include "nes/mapper.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
class Mapper001 : public Mapper {
 public:
  explicit Mapper001(Cartridge* cartridge);
  ~Mapper001() override;

 public:
  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  NametableMirroring GetNametableMirroring() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void WriteRegister(Address address, Byte value);

 private:
  bool uses_character_ram_ = false;
  Bytes character_ram_;

  Byte shift_register_ = 0;
  Byte write_count_ = 0;

  // (0: switch 8 KB at a time; 1: switch two separate 4 KB banks)
  Byte chr_mode_ = 0;

  // 0, 1: switch 32 KB at $8000, ignoring low bit of bank number;
  // 2: fix first bank at $8000 and switch 16 KB bank at $C000;
  // 3: fix last bank at $C000 and switch 16 KB bank at $8000
  // Default PRG mode is 3.
  Byte prg_mode_ = 3;

  Byte chr_reg_0_ = 0;
  Byte chr_reg_1_ = 0;
  Byte prg_reg_ = 0;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER001_H_
