// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER079_H_
#define NES_MAPPERS_MAPPER079_H_

#include <cstddef>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// American Video Entertainment NINA-03/NINA-06.
// https://www.nesdev.org/wiki/INES_Mapper_079
class Mapper079 : public Mapper {
 public:
  explicit Mapper079(Cartridge* cartridge);
  ~Mapper079() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  NametableMirroring GetNametableMirroring() override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  Mapper079(Cartridge* cartridge, bool multicart_mode);

 private:
  static constexpr size_t kCHRRAMSize = 0x2000;

  void SelectBanks(Byte value);

  const bool multicart_mode_;
  Byte selected_prg_bank_ = 0;
  Byte selected_chr_bank_ = 0;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER079_H_
