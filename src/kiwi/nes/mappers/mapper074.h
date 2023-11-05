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

#ifndef NES_MAPPERS_MAPPER074_H_
#define NES_MAPPERS_MAPPER074_H_

#include "nes/mappers/mapper004.h"

#include <memory>

namespace kiwi {
namespace nes {
// The circuit board mounts an MMC3 clone together with a 74LS138 and 74LS139 to
// redirect 1 KiB CHR-ROM banks #8 and #9 to 2 KiB of CHR-RAM.
class Mapper074 : public Mapper004 {
 public:
  explicit Mapper074(Cartridge* cartridge);
  ~Mapper074() override;

 public:
  // Mapper003:
  void WriteCHR(Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  Byte ReadCHRByBank(int bank, Address address) override;

 private:
  Bytes prg_ram_;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER074_H_
