// Copyright (C) 2025 Yisi Yu
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

#ifndef NES_MAPPERS_MAPPER009_H_
#define NES_MAPPERS_MAPPER009_H_

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {
// https://www.nesdev.org/wiki/MMC2
class Mapper009 : public Mapper {
 public:
  explicit Mapper009(Cartridge* cartridge);
  ~Mapper009() override;

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
  Address latch_0_ = 0xfe;
  Address latch_1_ = 0xfe;
  Address select_chr_first_ = 0;
  Address select_chr_second_ = 0;
  Address chr_regs_[4]{0, 4, 0, 0};
  Address select_prg_ = 0;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};
}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER009_H_
