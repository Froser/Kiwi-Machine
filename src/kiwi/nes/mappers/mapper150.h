// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER150_H_
#define NES_MAPPERS_MAPPER150_H_

#include <cstddef>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Sachen 74LS374N ASIC on the SA-015/SA-630 and SA-020A boards.
// https://www.nesdev.org/wiki/INES_Mapper_150
// https://www.nesdev.org/wiki/INES_Mapper_243
class Mapper150 : public Mapper {
 public:
  explicit Mapper150(Cartridge* cartridge);
  ~Mapper150() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;

  NametableMirroring GetNametableMirroring() override;
  bool UsesCustomPPUMemoryMapping() const override;
  Byte ReadPPUMemoryByte(Byte* ciram, Address address) override;
  void WritePPUMemoryByte(Byte* ciram, Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  void UpdateState();
  Byte GetNametablePage(Address address) const;

  Byte current_register_ = 0;
  Byte registers_[8]{};
  Byte selected_prg_bank_ = 0;
  Byte selected_chr_bank_ = 0;
  Byte mirroring_mode_ = 0;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER150_H_
