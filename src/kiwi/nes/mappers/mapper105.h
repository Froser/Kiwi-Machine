// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER105_H_
#define NES_MAPPERS_MAPPER105_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// NES-EVENT, the MMC1-based board used by Nintendo World Championships 1990.
// https://www.nesdev.org/wiki/INES_Mapper_105
class Mapper105 : public Mapper {
 public:
  explicit Mapper105(Cartridge* cartridge);
  ~Mapper105() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;

  NametableMirroring GetNametableMirroring() override;
  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void WriteRegister(Address address, Byte value);
  void UpdateEventState();
  size_t ResolvePRGBank(Address address) const;

  static constexpr size_t kCHRRAMSize = 0x2000;
  static constexpr uint32_t kIRQThreshold = 0x20000000;

  size_t prg_bank_count_ = 0;
  Bytes character_ram_;

  Byte shift_register_ = 0;
  Byte write_count_ = 0;
  Byte control_ = 0x0c;
  Byte chr_reg_0_ = 0x10;
  Byte chr_reg_1_ = 0;
  Byte prg_reg_ = 0;
  Byte initialization_state_ = 0;
  bool wrote_prg_this_cycle_ = false;
  bool wram_disabled_ = false;
  bool irq_enabled_ = false;
  uint32_t irq_counter_ = 0;
  NametableMirroring mirroring_ = NametableMirroring::kOneScreenLower;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER105_H_
