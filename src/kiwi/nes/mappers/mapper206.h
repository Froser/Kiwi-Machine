// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER206_H_
#define NES_MAPPERS_MAPPER206_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"

namespace kiwi {
namespace nes {

// Namcot 108 family and its mapper 76/88/95/154 board variants.
// https://www.nesdev.org/wiki/INES_Mapper_206
class Mapper206 : public Mapper {
 public:
  explicit Mapper206(Cartridge* cartridge);
  ~Mapper206() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  NametableMirroring GetNametableMirroring() override;
  bool UsesCustomPPUMemoryMapping() const override;
  Byte ReadPPUMemoryByte(Byte* ciram, Address address) override;
  void WritePPUMemoryByte(Byte* ciram, Address address, Byte value) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 private:
  enum class Variant : Byte {
    kNamco108,
    kNamco3446,
    kNamco108_88,
    kNamco3425,
    kNamco3453,
  };

  void ResetRegisters();
  size_t GetCHRAddress(Address address) const;
  Byte ReadCHRMemory(Address address);
  void WriteCHRMemory(Address address, Byte value);
  Byte ReadPRGBank(Byte bank, Address address);

  Variant variant_ = Variant::kNamco108;
  Byte selected_register_ = 0;
  Byte bank_registers_[8]{};
  Byte nametable_pages_[2]{};
  size_t prg_bank_count_ = 0;
  size_t chr_bank_count_ = 0;
  bool uses_character_ram_ = false;
  Bytes character_ram_;
  Bytes four_screen_ram_;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER206_H_
