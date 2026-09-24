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

#ifndef NES_MAPPERS_MAPPER041_H_
#define NES_MAPPERS_MAPPER041_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Caltron 6-in-1.
// https://www.nesdev.org/wiki/INES_Mapper_041
class Mapper041 : public Mapper {
 public:
  explicit Mapper041(Cartridge* cartridge);
  ~Mapper041() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;

  NametableMirroring GetNametableMirroring() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void SetMirroring(NametableMirroring mirroring);

  Byte selected_prg_bank_ = 0;
  Byte selected_chr_bank_ = 0;
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  NametableMirroring mirroring_ = NametableMirroring::kVertical;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER041_H_
