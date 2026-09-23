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

#ifndef NES_MAPPERS_MAPPER064_H_
#define NES_MAPPERS_MAPPER064_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Tengen RAMBO-1.
// https://www.nesdev.org/wiki/RAMBO-1
class Mapper064 : public Mapper {
 public:
  explicit Mapper064(Cartridge* cartridge);
  ~Mapper064() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  NametableMirroring GetNametableMirroring() override;

  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;
  void ScanlineIRQ(int scanline, bool render_enabled) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void ResetRegisters();
  void ClockIRQCounter(Byte delay);
  size_t GetCHRAddress(Address address) const;
  Byte ReadPRGBank(Byte bank, Address address);

  std::array<Byte, 16> bank_registers_{};
  Byte selected_register_ = 0;
  bool one_k_chr_mode_ = false;
  bool prg_mode_ = false;
  bool chr_mode_ = false;

  bool irq_enabled_ = false;
  bool irq_cycle_mode_ = false;
  bool irq_reload_pending_ = false;
  Byte irq_counter_ = 0;
  Byte irq_reload_value_ = 0;
  Byte cpu_clock_counter_ = 0;
  Byte irq_delay_ = 0;
  bool force_cpu_clock_ = false;

  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  bool uses_character_ram_ = false;
  Bytes character_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER064_H_
