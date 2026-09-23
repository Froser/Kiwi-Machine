// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER132_H_
#define NES_MAPPERS_MAPPER132_H_

#include <cstddef>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// TXC 01-22003/22111/22270 boards using the 05-00002-010 ASIC.
// https://www.nesdev.org/wiki/INES_Mapper_132
class Mapper132 : public Mapper {
 public:
  explicit Mapper132(Cartridge* cartridge);
  ~Mapper132() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void UpdateBanks();

  Byte accumulator_ = 0;
  Byte staging_ = 0;
  Byte output_ = 0;
  bool s_flag_ = false;
  bool increment_ = false;
  bool invert_ = false;
  Byte selected_prg_bank_ = 0;
  Byte selected_chr_bank_ = 0;
  size_t chr_bank_count_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER132_H_
