// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER168_H_
#define NES_MAPPERS_MAPPER168_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// RacerMate Challenge II board.
// https://www.nesdev.org/wiki/INES_Mapper_168
class Mapper168 : public Mapper {
 public:
  explicit Mapper168(Cartridge* cartridge);
  ~Mapper168() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  NametableMirroring GetNametableMirroring() override;

  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;
  size_t GetCustomNVRAMSize() const override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  Bytes CopyPRGNVRAM() override;
  bool RestorePRGNVRAM(const Bytes& data) override;

 private:
  bool IsProtectedCHRAddress(size_t address) const;
  void WriteIRQControl(Address address, Byte value);

  size_t prg_bank_count_ = 0;
  size_t chr_nvram_size_ = 0;
  Byte selected_prg_bank_ = 0;
  Byte selected_chr_bank_ = 0;
  uint16_t irq_counter_ = 0;
  bool irq_disabled_ = false;
  bool irq_asserted_ = false;
  bool chr_nvram_unlocked_ = false;
  Bytes character_ram_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER168_H_
