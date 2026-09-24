// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER016_H_
#define NES_MAPPERS_MAPPER016_H_

#include <cstddef>
#include <cstdint>
#include <memory>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

class BandaiEEPROM;

// Bandai FCG-1/2, LZ93D50, Datach, and related boards.
// https://www.nesdev.org/wiki/INES_Mapper_016
class Mapper016 : public Mapper {
 public:
  explicit Mapper016(Cartridge* cartridge);
  ~Mapper016() override;

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
  bool UsesCustomPRGRAM() const override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  Bytes CopyPRGNVRAM() override;
  bool RestorePRGNVRAM(const Bytes& data) override;

 private:
  void ResetRegisters();
  bool CanWriteRegister(Address address);
  void WriteRegister(Address address, Byte value);
  Byte ReadRegister(Address address) const;
  Byte ReadPRGBank(Byte bank, Address address);

  Byte prg_page_ = 0;
  Byte prg_bank_select_ = 0;
  Byte chr_banks_[8]{};
  uint16_t irq_counter_ = 0;
  uint16_t irq_reload_ = 0;
  bool irq_enabled_ = false;
  bool ram_enabled_ = false;
  bool uses_character_ram_ = false;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
  std::unique_ptr<BandaiEEPROM> standard_eeprom_;
  std::unique_ptr<BandaiEEPROM> extra_eeprom_;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER016_H_
