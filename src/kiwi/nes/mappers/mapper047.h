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

#ifndef NES_MAPPERS_MAPPER047_H_
#define NES_MAPPERS_MAPPER047_H_

#include "nes/mappers/mapper004.h"

namespace kiwi {
namespace nes {

// Nintendo QJ: MMC3 with two 128 KiB PRG and CHR blocks.
// https://www.nesdev.org/wiki/INES_Mapper_047
class Mapper047 : public Mapper004 {
 public:
  explicit Mapper047(Cartridge* cartridge);
  ~Mapper047() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;
  void WriteExtendedRAM(Address address, Byte value) override;

  uint32_t GetAbsoluteCHRAddress(Address address) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  Byte ReadCHRByBank(int bank, Address address) override;

 private:
  int MapPRGBank(int bank) const;
  int MapCHRBank(int bank) const;

  Byte selected_block_ = 0;
  bool work_ram_enabled_ = false;
  bool work_ram_write_protected_ = false;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER047_H_
