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

#ifndef NES_MAPPERS_MAPPER002_H_
#define NES_MAPPERS_MAPPER002_H_

#include "nes/mapper.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
// https://www.nesdev.org/wiki/INES_Mapper_002
// https://www.nesdev.org/wiki/CNROM
class Mapper002 : public Mapper {
 public:
  explicit Mapper002(Cartridge* cartridge);
  ~Mapper002() override;

 public:
  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  Byte* last_bank();

 private:
  bool uses_character_ram_ = false;
  int last_bank_offset_ = 0;
  Address select_prg_ = 0;
  Bytes character_ram_;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER002_H_
