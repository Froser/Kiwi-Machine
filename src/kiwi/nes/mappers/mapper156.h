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

#ifndef NES_MAPPERS_MAPPER156_H_
#define NES_MAPPERS_MAPPER156_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Daou Infosys DIS23C01.
// https://www.nesdev.org/wiki/INES_Mapper_156
class Mapper156 : public Mapper {
 public:
  explicit Mapper156(Cartridge* cartridge);
  ~Mapper156() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  NametableMirroring GetNametableMirroring() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void SetMirroring(Byte value);

  Byte selected_prg_bank_ = 0;
  Byte chr_low_[8]{};
  Byte chr_high_[8]{};
  NametableMirroring mirroring_ = NametableMirroring::kOneScreenLower;
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER156_H_
