// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER178_H_
#define NES_MAPPERS_MAPPER178_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Waixing FS-305.
// https://www.nesdev.org/wiki/INES_Mapper_178
class Mapper178 : public Mapper {
 public:
  explicit Mapper178(Cartridge* cartridge);
  ~Mapper178() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;

  NametableMirroring GetNametableMirroring() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  size_t GetPRGBank(Address address) const;
  size_t GetWorkRAMAddress(Address address) const;
  void UpdateMirroring();

  Byte registers_[4]{};
  size_t prg_bank_count_ = 0;
  size_t work_ram_bank_count_ = 0;
  Bytes character_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kVertical;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER178_H_
