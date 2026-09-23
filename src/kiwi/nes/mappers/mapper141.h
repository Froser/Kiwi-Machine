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

#ifndef NES_MAPPERS_MAPPER141_H_
#define NES_MAPPERS_MAPPER141_H_

#include <array>
#include <cstddef>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Sachen 8259A and the Q-Boy 2M-RAM-COB CHR-RAM variant.
// https://www.nesdev.org/wiki/INES_Mapper_141
class Mapper141 : public Mapper {
 public:
  explicit Mapper141(Cartridge* cartridge);
  ~Mapper141() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;

  NametableMirroring GetNametableMirroring() override;
  bool UsesCustomPPUMemoryMapping() const override;
  Byte ReadPPUMemoryByte(Byte* ciram, Address address) override;
  void WritePPUMemoryByte(Byte* ciram, Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  Byte GetMirroringMode() const;
  Byte GetNametablePage(Address address) const;

  Byte current_register_ = 0;
  std::array<Byte, 8> registers_{};
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER141_H_
