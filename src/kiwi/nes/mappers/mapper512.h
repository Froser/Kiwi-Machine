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

#ifndef NES_MAPPERS_MAPPER512_H_
#define NES_MAPPERS_MAPPER512_H_

#include "nes/mappers/mapper004.h"

namespace kiwi {
namespace nes {

// Sachen Zhongguo Daheng board: MMC3 with separate CHR-RAM and cartridge VRAM.
// https://www.nesdev.org/wiki/NES_2.0_Mapper_512
class Mapper512 : public Mapper004 {
 public:
  explicit Mapper512(Cartridge* cartridge);
  ~Mapper512() override;

  void Reset() override;
  void WriteExtendedRAM(Address address, Byte value) override;
  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;
  NametableMirroring GetNametableMirroring() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  bool UsesCHRRAM() const;
  bool UsesCartridgeVRAM() const;
  void ResetMMC3Registers();

  Byte mode_ = 0;
  Bytes character_ram_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER512_H_
