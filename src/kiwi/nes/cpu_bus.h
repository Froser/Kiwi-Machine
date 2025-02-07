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

#ifndef NES_CPU_BUS_H_
#define NES_CPU_BUS_H_

#include <vector>

#include "nes/emulator.h"
#include "nes/emulator_states.h"
#include "nes/registers.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {
class Mapper;
class Device;

// CPU Bus is connected to CPU.
// See https://www.nesdev.org/wiki/CPU_memory_map for more addressing details.
class CPUBus : public EmulatorStates::SerializableState {
 public:
  CPUBus();
  ~CPUBus() override;

  // Bus:
  void SetMapper(Mapper* mapper);
  Mapper* GetMapper();
  Byte Read(Address address);
  void Write(Address address, Byte value);
  Byte* GetPagePointer(Byte page);
  Word ReadWord(Address address);

  void set_ppu(Device* ppu) { ppu_ = ppu; }
  void set_emulator(Device* emulator) { emulator_ = emulator; }

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  Mapper* mapper_ = nullptr;
  Device* ppu_ = nullptr;
  Device* emulator_ = nullptr;
  Byte ram_[0x800] = {0};
};

}  // namespace nes
}  // namespace kiwi

#endif  // AHA_COMPONENTS_KIWI_NES_CORE_CPU_BUS_H_
