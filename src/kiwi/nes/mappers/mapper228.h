// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER228_H_
#define NES_MAPPERS_MAPPER228_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Active Enterprises board used by Action 52 and Cheetahmen II.
// https://www.nesdev.org/wiki/INES_Mapper_228
class Mapper228 : public Mapper {
 public:
  explicit Mapper228(Cartridge* cartridge);
  ~Mapper228() override;

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
  void SelectBanks(Address address, Byte value);

  Byte prg_banks_[2]{};
  Byte chr_bank_ = 0;
  NametableMirroring mirroring_ = NametableMirroring::kVertical;
  bool prg_open_bus_ = false;
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER228_H_
