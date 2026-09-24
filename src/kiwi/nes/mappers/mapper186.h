// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER186_H_
#define NES_MAPPERS_MAPPER186_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Fukutake Study Box BIOS mapper. Tape media is a separate peripheral format.
// https://www.nesdev.org/wiki/INES_Mapper_186
class Mapper186 : public Mapper {
 public:
  explicit Mapper186(Cartridge* cartridge);
  ~Mapper186() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;
  Byte* GetExtendedRAMPointer() override;
  bool UsesCustomPRGRAM() const override;

  NametableMirroring GetNametableMirroring() override;

  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  size_t GetWorkRAMAddress(Address address) const;
  Byte ReadRegister(Address address);
  void WriteRegister(Address address, Byte value);

  size_t prg_bank_count_ = 0;
  Byte selected_prg_bank_ = 0;
  Byte ram_control_ = 0;
  Byte tape_control_ = 0;
  Byte command_ = 0;
  Byte command_bit_count_ = 0;
  uint16_t ready_delay_ = 0;
  bool ready_for_bit_ = false;
  Bytes work_ram_;
  Bytes character_ram_;
  Bytes nametable_ram_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER186_H_
