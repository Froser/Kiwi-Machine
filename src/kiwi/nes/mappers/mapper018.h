// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER018_H_
#define NES_MAPPERS_MAPPER018_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Jaleco SS88006.
// https://www.nesdev.org/wiki/INES_Mapper_018
class Mapper018 : public Mapper {
 public:
  explicit Mapper018(Cartridge* cartridge);
  ~Mapper018() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  NametableMirroring GetNametableMirroring() override;

  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void ResetRegisters();
  void UpdatePRGBank(size_t bank, Byte value, bool upper_nibble);
  void UpdateCHRBank(size_t bank, Byte value, bool upper_nibble);

  Byte prg_banks_[3]{};
  Byte chr_banks_[8]{};
  Byte irq_reload_[4]{};
  uint16_t irq_counter_ = 0;
  Byte irq_counter_size_ = 0;
  bool irq_enabled_ = false;
  bool uses_character_ram_ = false;
  Bytes character_ram_;
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER018_H_
