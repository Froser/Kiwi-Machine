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

#ifndef NES_MAPPERS_MAPPER189_H_
#define NES_MAPPERS_MAPPER189_H_

#include "nes/mappers/mapper004.h"

namespace kiwi {
namespace nes {

// TXC/Yoko Soft board: MMC3 CHR/IRQ logic with 32 KiB PRG banking.
// https://www.nesdev.org/wiki/INES_Mapper_189
class Mapper189 : public Mapper004 {
 public:
  explicit Mapper189(Cartridge* cartridge);
  ~Mapper189() override;

  void Reset() override;

  Byte ReadPRG(Address address) override;
  void WriteExtendedRAM(Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  Byte selected_prg_bank_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER189_H_
