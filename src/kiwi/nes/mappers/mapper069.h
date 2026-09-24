// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER069_H_
#define NES_MAPPERS_MAPPER069_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Sunsoft FME-7 / Sunsoft 5A / Sunsoft 5B.
// https://www.nesdev.org/wiki/Sunsoft_FME-7
class Mapper069 : public Mapper {
 public:
  explicit Mapper069(Cartridge* cartridge);
  ~Mapper069() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

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
  void ResetRegisters();
  void WriteCommandParameter(Byte value);
  Byte ReadPRGBank(Byte bank, Address offset);

  Byte command_ = 0;
  Byte work_ram_value_ = 0;
  Byte prg_banks_[3]{};
  Byte chr_banks_[8]{};
  uint16_t irq_counter_ = 0;
  bool irq_enabled_ = false;
  bool irq_counter_enabled_ = false;
  bool uses_character_ram_ = false;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER069_H_
