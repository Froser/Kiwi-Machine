// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER067_H_
#define NES_MAPPERS_MAPPER067_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Sunsoft-3.
// https://www.nesdev.org/wiki/INES_Mapper_067
class Mapper067 : public Mapper {
 public:
  explicit Mapper067(Cartridge* cartridge);
  ~Mapper067() override;

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
  size_t GetCHRAddress(Address address) const;
  Byte ReadPRGBank(size_t bank, Address address);
  void SetMirroring(Byte value);

  Byte prg_bank_ = 0;
  std::array<Byte, 4> chr_banks_{};
  bool irq_write_low_ = false;
  bool irq_enabled_ = false;
  uint16_t irq_counter_ = 0;
  bool uses_character_ram_ = false;
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER067_H_
