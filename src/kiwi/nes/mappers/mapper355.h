// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER355_H_
#define NES_MAPPERS_MAPPER355_H_

#include <array>
#include <cstdint>

#include "nes/mappers/mapper000.h"

namespace kiwi {
namespace nes {

// Hwang Shinwei 3D-BLOCK board. The original PIC16C54 firmware is absent from
// legacy dumps, so its IRQ protection is represented by the established HLE.
// https://www.nesdev.org/wiki/NES_2.0_Mapper_355
class Mapper355 : public Mapper000 {
 public:
  explicit Mapper355(Cartridge* cartridge);
  ~Mapper355() override;

  void Reset() override;
  void WriteExtendedRAM(Address address, Byte value) override;
  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  static constexpr uint16_t kIRQPulseCycles = 0x10;

  std::array<Byte, 4> protection_registers_{};
  uint16_t irq_period_ = 0;
  uint16_t irq_countdown_ = 0;
  uint16_t irq_pulse_cycles_ = 0;
  bool irq_enabled_ = false;
  bool irq_asserted_ = false;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER355_H_
