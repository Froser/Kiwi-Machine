// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER068_H_
#define NES_MAPPERS_MAPPER068_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Sunsoft-4.
// https://www.nesdev.org/wiki/INES_Mapper_068
class Mapper068 : public Mapper {
 public:
  explicit Mapper068(Cartridge* cartridge);
  ~Mapper068() override;

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

  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  Byte GetNametablePage(Address address) const;
  size_t GetCHRAddress(Address address) const;
  Byte ReadSelectedPRGBank(Address address);

  Byte prg_bank_ = 0;
  Byte chr_banks_[4]{};
  Byte nametable_banks_[2]{};
  NametableMirroring mirroring_ = NametableMirroring::kVertical;
  bool use_chr_nametables_ = false;
  bool prg_ram_enabled_ = false;
  bool using_external_rom_ = false;
  bool has_external_rom_ = false;
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  uint32_t licensing_timer_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER068_H_
