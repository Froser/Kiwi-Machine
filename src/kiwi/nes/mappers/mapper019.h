// Copyright (C) 2026 Yisi Yu
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

#ifndef NES_MAPPERS_MAPPER019_H_
#define NES_MAPPERS_MAPPER019_H_

#include <cstddef>
#include <cstdint>

#include "nes/emulator_states.h"
#include "nes/mapper.h"
#include "nes/types.h"
#include "third_party/nes_apu/Nes_Namco.h"

namespace kiwi {
namespace nes {

// Namcot 129/163 and the cost-reduced Namcot 175/340 variants.
// https://www.nesdev.org/wiki/INES_Mapper_019
// https://www.nesdev.org/wiki/INES_Mapper_210
class Mapper019 : public Mapper {
 public:
  explicit Mapper019(Cartridge* cartridge);
  ~Mapper019() override;

  void Reset() override;

  void WritePRG(Address address, Byte value) override;
  Byte ReadPRG(Address address) override;

  void WriteCHR(Address address, Byte value) override;
  Byte ReadCHR(Address address) override;
  uint32_t GetAbsoluteCHRAddress(Address address) override;

  void WriteExtendedRAM(Address address, Byte value) override;
  Byte ReadExtendedRAM(Address address) override;
  Byte* GetExtendedRAMPointer() override;
  bool UsesCustomPRGRAM() const override;

  NametableMirroring GetNametableMirroring() override;
  bool NeedsM2CycleIRQ() const override;
  void M2CycleIRQ() override;

  bool UsesCustomPPUMemoryMapping() const override;
  Byte ReadPPUMemoryByte(Byte* ciram, Address address) override;
  void WritePPUMemoryByte(Byte* ciram, Address address, Byte value) override;

  void SetExpansionAudioOutput(Blip_Buffer* output) override;
  void SetExpansionAudioVolume(float volume) override;
  void EndExpansionAudioFrame(int64_t cycles) override;

  // EmulatorStates::SerializableState:
  void Serialize(EmulatorStates::SerializableStateData& data) override;
  bool Deserialize(const EmulatorStates::Header& header,
                   EmulatorStates::DeserializableStateData& data) override;

 protected:
  Bytes CopyPRGNVRAM() override;
  bool RestorePRGNVRAM(const Bytes& data) override;

 private:
  enum class Variant : Byte {
    kNamco163,
    kNamco175,
    kNamco340,
  };

  void ResetRegisters();
  bool MapsCIRAM(size_t window, Byte bank) const;
  size_t GetCHRAddress(Address address) const;
  Byte ReadPRGBank(Byte bank, Address address);
  bool CanWritePRGRAM(Address address) const;
  bool HasExpansionAudio();

  Variant variant_ = Variant::kNamco163;
  Byte prg_banks_[3]{};
  Byte chr_banks_[8]{};
  Byte nametable_banks_[4]{};
  Byte write_protect_ = 0;
  uint16_t irq_counter_ = 0;
  bool prg_ram_enabled_ = false;
  bool low_chr_ciram_disabled_ = false;
  bool high_chr_ciram_disabled_ = false;
  NametableMirroring mirroring_ = NametableMirroring::kHorizontal;
  size_t chr_bank_count_ = 0;
  size_t external_prg_nvram_size_ = 0;
  Bytes character_ram_;
  Bytes prg_ram_;
  Nes_Namco audio_;
  int64_t audio_cycles_ = 0;
};

}  // namespace nes
}  // namespace kiwi

#endif  // NES_MAPPERS_MAPPER019_H_
