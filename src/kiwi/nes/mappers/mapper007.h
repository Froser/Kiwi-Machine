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

#ifndef NES_MAPPERS_MAPPER007_H_
#define NES_MAPPERS_MAPPER007_H_

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
// https://www.nesdev.org/wiki/AxROM
class Mapper007 : public Mapper {
 public:
  explicit Mapper007(Cartridge* cartridge);
  ~Mapper007() override;

 public:
  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  NametableMirroring GetNametableMirroring() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  bool uses_character_ram_ = false;
  Bytes character_ram_;
  uint8_t select_prg_ = 0;
  uint8_t select_mirror_ = 0;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER000_H_
