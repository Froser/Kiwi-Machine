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

#ifndef NES_MAPPERS_MAPPER040_H_
#define NES_MAPPERS_MAPPER040_H_

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
// https://www.nesdev.org/wiki/NROM
class Mapper040 : public Mapper {
 public:
  explicit Mapper040(Cartridge* cartridge);
  ~Mapper040() override;

 public:
  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  // 6000-7fff: bank #6
  Byte ReadExtendedRAM(Address address) override;

  void M2CycleIRQ() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  bool uses_character_ram_ = false;
  Bytes character_ram_;
  Byte selected_bank_ = 0;
  bool irq_enabled_ = false;
  uint64_t irq_count_ = 0;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER040_H_
