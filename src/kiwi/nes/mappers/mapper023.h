// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER023_H_
#define NES_MAPPERS_MAPPER023_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Konami VRC2a/VRC2b/VRC2c/VRC4a/VRC4b/VRC4c/VRC4d/VRC4e/VRC4f.
// https://www.nesdev.org/wiki/VRC2_and_VRC4
class Mapper023 : public Mapper {
 public:
  explicit Mapper023(Cartridge* cartridge);
  ~Mapper023() override;

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
  enum class Wiring {
    kMapper023Heuristic,
#if BUILDFLAG(ENABLE_MAPPER_021)
    kMapper021Heuristic,
    kVRC4a,
    kVRC4c,
#endif
#if BUILDFLAG(ENABLE_MAPPER_022)
    kVRC2a,
#endif
#if BUILDFLAG(ENABLE_MAPPER_025)
    kMapper025Heuristic,
#endif
    kVRC4f,
    kVRC4e,
    kVRC2b,
#if BUILDFLAG(ENABLE_MAPPER_025)
    kVRC4b,
    kVRC4d,
    kVRC2c,
#endif
  };

  void ResetRegisters();
  Address TranslateAddress(Address address) const;
  bool SupportsVRC4Features() const;
  bool UsesVRC2Latch() const;
  Byte ReadPRGBank(Byte bank, Address address);
  void SetMirroring(Byte value);
  void SetIRQControl(Byte value);
  void AcknowledgeIRQ();

  Wiring wiring_ = Wiring::kMapper023Heuristic;
  Byte prg_bank_0_ = 0;
  Byte prg_bank_1_ = 0;
  bool prg_mode_ = false;
  Byte chr_low_[8]{};
  Byte chr_high_[8]{};
  Byte latch_ = 0;
  Byte irq_reload_ = 0;
  Byte irq_counter_ = 0;
  int16_t irq_prescaler_ = 0;
  bool irq_enabled_ = false;
  bool irq_enabled_after_ack_ = false;
  bool irq_cycle_mode_ = false;
  bool uses_character_ram_ = false;
  size_t chr_bank_count_ = 0;
  Bytes character_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER023_H_
