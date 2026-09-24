// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#include "nes/mappers/mapper355.h"

#include "base/check.h"
#include "nes/cartridge.h"

namespace kiwi {
namespace nes {

Mapper355::Mapper355(Cartridge* cartridge) : Mapper000(cartridge) {
  DCHECK_EQ(rom_data()->mapper, 355);
  DCHECK_EQ(rom_data()->PRG.size(), 0x8000u);
  DCHECK(rom_data()->CHR.empty());
  rom_data()->name_table_mirroring = NametableMirroring::kVertical;
}

Mapper355::~Mapper355() = default;

void Mapper355::Reset() {
  protection_registers_.fill(0);
  irq_period_ = static_cast<uint16_t>(irq_period_ + 0x10);
  irq_countdown_ = 0;
  irq_pulse_cycles_ = 0;
  irq_enabled_ = false;
  irq_asserted_ = false;
}

void Mapper355::WriteExtendedRAM(Address address, Byte value) {
  switch (address) {
    case 0x4800:
      protection_registers_[0] = value;
      break;
    case 0x4900:
      protection_registers_[1] = value;
      break;
    case 0x4a00:
      protection_registers_[2] = value;
      break;
    case 0x4e00:
      protection_registers_[3] = value;
      irq_countdown_ = irq_period_ == 0 ? 0x10 : irq_period_;
      irq_pulse_cycles_ = kIRQPulseCycles;
      irq_enabled_ = true;
      irq_asserted_ = false;
      break;
    default:
      break;
  }
}

bool Mapper355::NeedsM2CycleIRQ() const {
  return true;
}

void Mapper355::M2CycleIRQ() {
  if (!irq_enabled_)
    return;

  if (irq_countdown_ != 0) {
    --irq_countdown_;
    if (irq_countdown_ == 0) {
      irq_asserted_ = true;
      if (irq_callback())
        irq_callback().Run();
    }
    return;
  }

  if (irq_pulse_cycles_ != 0) {
    --irq_pulse_cycles_;
    return;
  }

  irq_countdown_ = irq_period_;
  irq_pulse_cycles_ = kIRQPulseCycles;
  irq_asserted_ = false;
}

void Mapper355::Serialize(EmulatorStates::SerializableStateData& data) {
  Mapper000::Serialize(data);
  data.WriteData(protection_registers_)
      .WriteData(irq_period_)
      .WriteData(irq_countdown_)
      .WriteData(irq_pulse_cycles_)
      .WriteData(irq_enabled_)
      .WriteData(irq_asserted_);
}

bool Mapper355::Deserialize(const EmulatorStates::Header& header,
                            EmulatorStates::DeserializableStateData& data) {
  if (!Mapper000::Deserialize(header, data))
    return false;
  data.ReadData(&protection_registers_)
      .ReadData(&irq_period_)
      .ReadData(&irq_countdown_)
      .ReadData(&irq_pulse_cycles_)
      .ReadData(&irq_enabled_)
      .ReadData(&irq_asserted_);
  return true;
}

}  // namespace nes
}  // namespace kiwi
