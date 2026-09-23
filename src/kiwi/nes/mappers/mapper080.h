// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER080_H_
#define NES_MAPPERS_MAPPER080_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Taito X1-005/X1-017 and the X1-005 mapper 207 wiring variant.
// https://www.nesdev.org/wiki/Taito_X1-005
// https://www.nesdev.org/wiki/Taito_X1-017
class Mapper080 : public Mapper {
 public:
  explicit Mapper080(Cartridge* cartridge);
  ~Mapper080() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;
  Byte* GetExtendedRAMPointer() override;

  NametableMirroring GetNametableMirroring() override;
  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;

  bool UsesCustomPPUMemoryMapping() const override;
  Byte ReadPPUMemoryByte(Byte* ciram, Address address) override;
  void WritePPUMemoryByte(Byte* ciram, Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  enum class Variant : Byte {
    kX1005,
    kX1017,
    kX1005Mapper207,
  };

  void ConfigureInternalRAM();
  void WriteX1005Register(Address address, Byte value);
  void WriteX1017Register(Address address, Byte value);
  size_t GetCHRBank(Address address) const;
  bool IsRAMAddressEnabled(Address address) const;
  void SetMirroring(NametableMirroring mirroring);
  void ReloadIRQCounter(bool acknowledge);

  Variant variant_ = Variant::kX1005;
  Byte prg_banks_[3]{};
  Byte chr_banks_[6]{};
  Byte chr_mode_ = 0;
  Byte ram_permissions_[3]{};
  Byte x1005_ram_permission_ = 0;
  Byte nametable_pages_[2]{};
  Byte irq_latch_ = 0;
  Byte irq_control_ = 0;
  uint16_t irq_counter_ = 0;
  bool irq_pending_ = false;
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER080_H_
