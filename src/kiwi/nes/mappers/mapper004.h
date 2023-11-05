// Copyright (C) 2023 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#ifndef NES_MAPPERS_MAPPER004_H_
#define NES_MAPPERS_MAPPER004_H_

#include "nes/mapper.h"
#include "nes/types.h"

#include <memory>

namespace kiwi {
namespace nes {

// https://www.nesdev.org/wiki/MMC3
class Mapper004 : public Mapper {
 public:
  explicit Mapper004(Cartridge* cartridge);
  ~Mapper004() override;

 public:
  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;

  // Some games (such as SMB3), has no extended RAM, but still write data to
  // $6000-$7fff, so this mapper allocated a dedicate PRG RAM area here.
  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;

  NametableMirroring GetNametableMirroring() override;
  void ScanlineIRQ() override;
  void PPUAddressChanged(Address address) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  virtual Byte ReadCHRByBank(int bank, Address address);

 private:
  void StepIRQCounter();

 protected:
  Byte target_register_mask_ = 0x07;

  bool uses_character_ram_ = false;
  Bytes character_ram_;

  Byte prg_banks_count_ = 0;
  Address last_vram_address_ = 0;
  Byte target_register_ = 0;
  bool prg_mode_ = false;
  bool chr_mode_ = false;
  uint32_t bank_register_[8]{0, 2, 4, 5, 6, 7, 0, 1};

  bool irq_enabled_ = false;
  Byte irq_counter_ = 0;
  Byte irq_latch_ = 0;
  bool irq_reload_ = false;

  Bytes prg_ram_;
  Bytes mirroring_ram_;

  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER004_H_
