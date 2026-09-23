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

#ifndef NES_MAPPERS_MAPPER115_H_
#define NES_MAPPERS_MAPPER115_H_

#include "nes/mappers/mapper004.h"

namespace kiwi {
namespace nes {

// Ka Sheng SFC-02B/SFC-03/SFC-004 MMC3 clone.
// https://www.nesdev.org/wiki/INES_Mapper_115
class Mapper115 : public Mapper004 {
 public:
  explicit Mapper115(Cartridge* cartridge);
  ~Mapper115() override;

  void Reset() override;
  Byte ReadPRG(Address address) override;
  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  Byte ReadCHRByBank(int bank, Address address) override;

 private:
  int ResolvePRGBank(Address address) const;
  int MapCHRBank(int bank) const;

  Byte mode_register_ = 0;
  Byte outer_chr_bank_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER115_H_
